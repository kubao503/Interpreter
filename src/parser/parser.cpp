#include "parser.hpp"

#include "magic_enum/magic_enum.hpp"

std::optional<BuiltInType> Parser::getCurrentTokenBuiltInType() const {
    auto name = magic_enum::enum_name(currentToken_.getType());

    static constexpr std::size_t suffixSize{3};
    if (name.size() < suffixSize)
        return std::nullopt;

    name.remove_suffix(suffixSize);
    return magic_enum::enum_cast<BuiltInType>(name);
}

std::optional<Type> Parser::getCurrentTokenType() const {
    if (currentToken_.getType() == Token::Type::ID)
        return std::get<std::string>(currentToken_.getValue());
    return getCurrentTokenBuiltInType();
}

ReturnType typeToReturnType(const Type& type) {
    return std::visit([](auto s) -> ReturnType { return s; }, type);
}

/// PROGRAM = STMTS
Program Parser::parseProgram() {
    auto statements = parseStatements();
    expectEndOfFile();
    return {.statements = std::move(statements)};
}

void Parser::expectEndOfFile() const {
    if (currentToken_.getType() != Token::Type::ETX)
        throw SyntaxException(currentToken_.getPosition(), "Unknown statement");
}

/// STMTS = { STMT }
Statements Parser::parseStatements() {
    Statements statements;
    while (auto statement = parseStatement())
        statements.push_back(std::move(statement));
    return statements;
}

/// STMT = IF_STMT
///      | WHILE_STMT
///      | RET_STMT
///      | PRINT_STMT
///      | CONST_VAR_DEF
///      | VOID_FUNC
///      | DEF_OR_ASGN
///      | BUILT_IN_DEF
///      | STRUCT_DEF
///      | VNT_DEF
PStatement Parser::parseStatement() {
    auto prevPosition = statementPosition_;
    statementPosition_ = currentToken_.getPosition();

    for (const auto& parser : statementParsers_) {
        if (auto statement = parser(*this)) {
            statementPosition_ = prevPosition;
            return statement;
        }
    }
    statementPosition_ = prevPosition;
    return nullptr;
}

/// IF_STMT = if DISJ '{' STMTS '}'
PStatement Parser::parseIfStatement() {
    if (currentToken_.getType() != Token::Type::IF_KW)
        return nullptr;
    consumeToken();

    auto condition = parseDisjunctionExpression();
    if (!condition)
        throw SyntaxException(currentToken_.getPosition(),
                              "Expected if-statement condition");

    expect(Token::Type::L_C_BR,
           SyntaxException(currentToken_.getPosition(), "Missing left curly brace"));

    auto statements = parseStatements();

    expect(Token::Type::R_C_BR,
           SyntaxException(currentToken_.getPosition(), "Missing right curly brace"));

    return std::make_unique<IfStatement>(std::move(condition), std::move(statements),
                                         statementPosition_);
}

/// WHILE_STMT = while DISJ '{' STMTS '}'
PStatement Parser::parseWhileStatement() {
    if (currentToken_.getType() != Token::Type::WHILE_KW)
        return nullptr;
    consumeToken();

    auto condition = parseDisjunctionExpression();
    if (!condition)
        throw SyntaxException(currentToken_.getPosition(),
                              "Expected while-statement condition");

    expect(Token::Type::L_C_BR,
           SyntaxException(currentToken_.getPosition(), "Missing left curly brace"));

    auto statements = parseStatements();

    expect(Token::Type::R_C_BR,
           SyntaxException(currentToken_.getPosition(), "Missing right curly brace"));

    return std::make_unique<WhileStatement>(std::move(condition), std::move(statements),
                                            statementPosition_);
}

/// RET_STMT = return [ EXPR ] ';'
PStatement Parser::parseReturnStatement() {
    if (currentToken_.getType() != Token::Type::RETURN_KW)
        return nullptr;
    consumeToken();

    auto expression = parseExpression();

    expect(Token::Type::SEMI,
           SyntaxException(currentToken_.getPosition(),
                           "Missing semicolon after return statement"));

    return std::make_unique<ReturnStatement>(std::move(expression), statementPosition_);
}

/// PRINT_STMT = print [ EXPR ] ';'
PStatement Parser::parsePrintStatement() {
    if (currentToken_.getType() != Token::Type::PRINT_KW)
        return nullptr;
    consumeToken();

    auto expression = parseExpression();

    expect(Token::Type::SEMI, SyntaxException(currentToken_.getPosition(),
                                              "Missing semicolon after print statement"));

    return std::make_unique<PrintStatement>(std::move(expression), statementPosition_);
}

/// CONST_VAR_DEF = const TYPE ID ASGN
PStatement Parser::parseConstVarDef() {
    if (currentToken_.getType() != Token::Type::CONST_KW)
        return nullptr;
    const auto position = currentToken_.getPosition();
    consumeToken();

    const auto type = getCurrentTokenType();
    if (!type)
        throw SyntaxException(currentToken_.getPosition(), "Expected variable type");
    consumeToken();

    auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected variable name"));

    auto assignment = parseAssignment(name);

    return std::make_unique<VarDef>(true, *type, std::move(name),
                                    std::move(assignment->rhs), std::move(position));
}

/// VOID_FUNC = void ID FUNC_DEF
PStatement Parser::parseVoidFunc() {
    if (currentToken_.getType() != Token::Type::VOID_KW)
        return nullptr;
    consumeToken();

    const auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected function name"));

    return parseFuncDef(VoidType(), name);
}

/// DEF_OR_ASGN = ID ( FIELD_ASGN
///                  | DEF
///                  | FUNC_CALL ';' )
PStatement Parser::parseDefOrAssignment() {
    if (currentToken_.getType() != Token::Type::ID)
        return nullptr;

    auto name = std::get<std::string>(currentToken_.getValue());
    consumeToken();

    if (auto def = parseDef(name))
        return def;
    if (auto funcCall = parseFuncCall(name)) {
        expect(Token::Type::SEMI,
               SyntaxException(currentToken_.getPosition(),
                               "Missing semicolon after function call"));
        return funcCall;
    }
    return parseFieldAssignment(name);
}

/// FIELD_ASGN = { '.' ID } ASGN
PStatement Parser::parseFieldAssignment(const std::string& name) {
    LValue lvalue{name};

    while (currentToken_.getType() == Token::Type::DOT) {
        consumeToken();

        auto field = expectAndReturnValue<std::string>(
            Token::Type::ID, SyntaxException(currentToken_.getPosition(),
                                             "Expected field name after dot operator"));

        lvalue = std::unique_ptr<FieldAccess>(
            new FieldAccess{.container = std::move(lvalue), .field = std::move(field)});
    }

    return parseAssignment(std::move(lvalue));
}

/// ASGN = '=' EXPR ';'
std::unique_ptr<Assignment> Parser::parseAssignment(LValue lvalue) {
    expect(Token::Type::ASGN_OP,
           SyntaxException(currentToken_.getPosition(), "Expected assignment operator"));

    auto expression = parseExpression();
    if (!expression)
        throw SyntaxException(currentToken_.getPosition(),
                              "Expected expression after assignment");

    expect(Token::Type::SEMI,
           SyntaxException(currentToken_.getPosition(), "Missing semicolon"));

    return std::make_unique<Assignment>(std::move(lvalue), std::move(expression),
                                        statementPosition_);
}

/// BUILT_IN_DEF = BUILT_IN_TYPE DEF
PStatement Parser::parseBuiltInDef() {
    const auto type = getCurrentTokenBuiltInType();
    if (!type)
        return nullptr;

    consumeToken();
    return parseDef(*type);
}

/// DEF = ID ( FUNC_DEF | ASGN )
PStatement Parser::parseDef(const Type& type) {
    if (currentToken_.getType() != Token::Type::ID)
        return nullptr;
    const auto name = std::get<std::string>(currentToken_.getValue());
    consumeToken();

    const auto returnType = typeToReturnType(type);
    if (auto def = parseFuncDef(returnType, name))
        return def;
    auto assignment = parseAssignment(name);
    return std::make_unique<VarDef>(false, type, std::get<std::string>(assignment->lhs),
                                    std::move(assignment->rhs), statementPosition_);
}

/// FUNC_DEF = '(' PARAMS ')' '{' STMTS '}'
PStatement Parser::parseFuncDef(const ReturnType& returnType, const std::string& name) {
    if (currentToken_.getType() != Token::Type::L_PAR)
        return nullptr;
    consumeToken();

    auto parameters = parseList<Parameter>(&Parser::parseParameter);

    expect(Token::Type::R_PAR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing right parenthesis after function parameter list"));
    expect(Token::Type::L_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing left curly brace before function body"));

    auto statements = parseStatements();

    expect(Token::Type::R_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing right curly brace after function body"));
    return std::make_unique<FuncDef>(returnType, name, std::move(parameters),
                                     std::move(statements), statementPosition_);
}

/// PARAM = [ ref ] TYPE ID
std::optional<Parameter> Parser::parseParameter() {
    const auto position = currentToken_.getPosition();

    const bool ref{currentToken_.getType() == Token::Type::REF_KW};
    if (ref)
        consumeToken();

    const auto type = getCurrentTokenType();
    if (!type) {
        if (ref)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected parameter type after ref keyword");
        return std::nullopt;
    }
    consumeToken();

    const auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected parameter name"));

    return Parameter{.type = *type, .name = name, .ref = ref, .position = position};
}

/// FUNC_CALL = '(' ARGS ')'
std::unique_ptr<FuncCall> Parser::parseFuncCall(const std::string& name) {
    if (currentToken_.getType() != Token::Type::L_PAR)
        return nullptr;
    consumeToken();

    auto arguments = parseList<Argument>(&Parser::parseArgument);

    expect(Token::Type::R_PAR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing right parenthesis after function call arguments"));
    return std::make_unique<FuncCall>(name, std::move(arguments), statementPosition_);
}

/// STRUCT_DEF = struct ID '{' FIELDS '}'
PStatement Parser::parseStructDef() {
    if (currentToken_.getType() != Token::Type::STRUCT_KW)
        return nullptr;
    consumeToken();

    auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected struct name"));

    expect(Token::Type::L_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing left curly brace in struct difinition"));

    auto fields = parseList<Field>(&Parser::parseField);

    expect(Token::Type::R_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing right curly brace in struct difinition"));
    return std::make_unique<StructDef>(std::move(name), std::move(fields),
                                       statementPosition_);
}

/// VNT_DEF = variant ID '{' TYPES '}'
PStatement Parser::parseVariantDef() {
    if (currentToken_.getType() != Token::Type::VARIANT_KW)
        return nullptr;
    consumeToken();

    auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected variant name"));

    expect(Token::Type::L_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing left curly brace in variant difinition"));

    auto types = parseList<Type>(&Parser::parseType);
    if (types.empty())
        throw NoTypesInVariant{currentToken_.getPosition()};

    expect(Token::Type::R_C_BR,
           SyntaxException(currentToken_.getPosition(),
                           "Missing right curly brace in variant difinition"));

    return std::make_unique<VariantDef>(std::move(name), std::move(types),
                                        statementPosition_);
}

std::optional<Type> Parser::parseType() {
    const auto type = getCurrentTokenType();
    if (!type)
        return std::nullopt;
    consumeToken();
    return type;
}

/// FIELD = TYPE ID
std::optional<Field> Parser::parseField() {
    auto type = getCurrentTokenType();
    if (!type)
        return std::nullopt;
    consumeToken();

    auto name = expectAndReturnValue<std::string>(
        Token::Type::ID,
        SyntaxException(currentToken_.getPosition(), "Expected field name"));

    return Field{.type = *type, .name = std::move(name)};
}

/// EXPR = DISJ | STRUCT_INIT
PExpression Parser::parseExpression() {
    if (auto expr = parseStructInitExpression())
        return expr;
    return parseDisjunctionExpression();
}

/// STRUCT_INIT = '{' { EXPRS } '}'
PExpression Parser::parseStructInitExpression() {
    if (currentToken_.getType() != Token::Type::L_C_BR)
        return nullptr;
    const auto position = currentToken_.getPosition();
    consumeToken();

    auto exprs = parseExpressionList();

    expect(
        Token::Type::R_C_BR,
        SyntaxException(currentToken_.getPosition(),
                        "Missing right curly brace at the end of struct initialization"));
    return std::make_unique<StructInitExpression>(std::move(exprs), position);
}

/// EXPRS = [ EXPR { ',' EXPR } ]
std::vector<PExpression> Parser::parseExpressionList() {
    std::vector<PExpression> exprs;

    auto expr = parseExpression();
    if (!expr)
        return exprs;

    exprs.push_back(std::move(expr));

    while (currentToken_.getType() == Token::Type::CMA) {
        consumeToken();
        expr = parseExpression();
        if (!expr)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after comma");
        exprs.push_back(std::move(expr));
    }
    return exprs;
}

/// DISJ = CONJ { or CONJ }
PExpression Parser::parseDisjunctionExpression() {
    const auto position = currentToken_.getPosition();
    PExpression leftLogicFactor = parseConjunctionExpression();
    if (!leftLogicFactor)
        return nullptr;

    while (currentToken_.getType() == Token::Type::OR_KW) {
        consumeToken();
        auto rightLogicFactor = parseConjunctionExpression();
        if (!rightLogicFactor)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after 'or' keyword");
        leftLogicFactor = std::make_unique<DisjunctionExpression>(
            std::move(leftLogicFactor), std::move(rightLogicFactor), position);
    }

    return leftLogicFactor;
}

/// CONJ = EQ { and EQ }
PExpression Parser::parseConjunctionExpression() {
    const auto position = currentToken_.getPosition();
    auto leftLogicFactor = parseEqualExpression();
    if (!leftLogicFactor)
        return nullptr;

    while (currentToken_.getType() == Token::Type::AND_KW) {
        consumeToken();
        auto rightLogicFactor = parseEqualExpression();
        if (!rightLogicFactor)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after 'and' keyword");
        leftLogicFactor = std::make_unique<ConjunctionExpression>(
            std::move(leftLogicFactor), std::move(rightLogicFactor), position);
    }

    return leftLogicFactor;
}

/// EQ = REL [ '==' REL ]
///    | REL [ '!=' REL ]
PExpression Parser::parseEqualExpression() {
    const auto position = currentToken_.getPosition();
    auto leftEqFactor = parseRelExpression();
    if (!leftEqFactor)
        return nullptr;

    if (const auto& ctor = ComparisonExpression::getCtor(currentToken_.getType())) {
        consumeToken();
        auto rightEqFactor = parseRelExpression();
        if (!rightEqFactor)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after (not)equal operator");
        leftEqFactor =
            (*ctor)(std::move(leftEqFactor), std::move(rightEqFactor), position);
    }

    return leftEqFactor;
}

/// REL = ADD [ '<' ADD ]
///     | ADD [ '>' ADD ]
///     | ADD [ '<=' ADD ]
///     | ADD [ '>=' ADD ]
PExpression Parser::parseRelExpression() {
    const auto position = currentToken_.getPosition();
    auto leftRelFactor = parseAdditiveExpression();
    if (!leftRelFactor)
        return nullptr;

    if (const auto& ctor = RelationExpression::getCtor(currentToken_.getType())) {
        consumeToken();
        auto rightRelFactor = parseAdditiveExpression();
        if (!rightRelFactor)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after relation operator");
        leftRelFactor =
            (*ctor)(std::move(leftRelFactor), std::move(rightRelFactor), position);
    }

    return leftRelFactor;
}

/// ADD = TERM { '+' TERM }
///     | TERM { '-' TERM }
PExpression Parser::parseAdditiveExpression() {
    const auto position = currentToken_.getPosition();
    auto leftTerm = parseMultiplicativeExpression();
    if (!leftTerm)
        return nullptr;

    while (const auto& ctor = AdditionExpression::getCtor(currentToken_.getType())) {
        consumeToken();
        auto rightTerm = parseMultiplicativeExpression();
        if (!rightTerm)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after additive operator");
        leftTerm = (*ctor)(std::move(leftTerm), std::move(rightTerm), position);
    }

    return leftTerm;
}

/// TERM = FACTOR { '*' FACTOR }
///      | FACTOR { '/' FACTOR }
PExpression Parser::parseMultiplicativeExpression() {
    const auto position = currentToken_.getPosition();
    auto leftFactor = parseNegationExpression();
    if (!leftFactor)
        return nullptr;

    while (const auto& ctor =
               MultiplicativeExpression::getCtor(currentToken_.getType())) {
        consumeToken();
        auto rightFactor = parseNegationExpression();
        if (!rightFactor)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected expression after multiplicative operator");
        leftFactor = (*ctor)(std::move(leftFactor), std::move(rightFactor), position);
    }

    return leftFactor;
}

/// FACTOR = [ '-' | not ] UNARY
PExpression Parser::parseNegationExpression() {
    const auto position = currentToken_.getPosition();
    const auto& ctor = NegationExpression::getCtor(currentToken_.getType());
    if (ctor)
        consumeToken();

    auto expr = parseTypeExpression();
    if (ctor)
        return (*ctor)(std::move(expr), position);
    return expr;
}

/// UNARY = SRC [ as TYPE ]
///       | SRC [ is TYPE ]
PExpression Parser::parseTypeExpression() {
    auto expr = parseFieldAccessExpression();
    if (!expr)
        return nullptr;

    if (const auto& ctor = TypeExpression::getCtor(currentToken_.getType())) {
        const auto position = currentToken_.getPosition();
        consumeToken();

        auto type = getCurrentTokenType();
        consumeToken();
        if (!type)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected type after is/as keyword");

        expr = (*ctor)(std::move(expr), *type, position);
    }
    return expr;
}

/// SRC = CNTNR { '.' ID }
PExpression Parser::parseFieldAccessExpression() {
    const auto position = currentToken_.getPosition();
    auto expr = parseContainerExpression();
    if (!expr)
        return nullptr;

    while (currentToken_.getType() == Token::Type::DOT) {
        consumeToken();
        auto field = expectAndReturnValue<std::string>(
            Token::Type::ID, SyntaxException(currentToken_.getPosition(),
                                             "Expected field name after dot operator"));
        expr = std::make_unique<FieldAccessExpression>(std::move(expr), std::move(field),
                                                       position);
    }

    return expr;
}

/// CNTNR = '(' EXPR ')'
///       | CONST
///       | CALL_OR_VAR
PExpression Parser::parseContainerExpression() {
    if (auto expr = parseNestedExpression())
        return expr;
    if (auto expr = parseConstant())
        return expr;
    return parseVariableAccessOrFuncCall();
}

PExpression Parser::parseNestedExpression() {
    if (currentToken_.getType() != Token::Type::L_PAR)
        return nullptr;
    consumeToken();

    auto expr = parseExpression();

    expect(Token::Type::R_PAR,
           SyntaxException(currentToken_.getPosition(),
                           "Expected right parenthesis after nested expression"));
    return expr;
}

struct TokenValueToConstantValue {
    Constant::Value operator()(const std::monostate&) const {
        throw std::runtime_error("Expected token to have value");
    }
    Constant::Value operator()(const auto& v) const { return v; }
};

PExpression Parser::parseConstant() {
    if (!currentToken_.isConstant())
        return nullptr;

    const auto value = std::visit(TokenValueToConstantValue(), currentToken_.getValue());
    const auto position = currentToken_.getPosition();
    consumeToken();
    return std::make_unique<Constant>(value, position);
}

/// CALL_OR_VAR = ID [ '(' ARGS ')' ]
PExpression Parser::parseVariableAccessOrFuncCall() {
    if (currentToken_.getType() != Token::Type::ID)
        return nullptr;

    const auto name = std::get<std::string>(currentToken_.getValue());
    auto position = currentToken_.getPosition();
    consumeToken();

    if (auto funcCall = parseFuncCall(name))
        return funcCall;
    return std::make_unique<VariableAccess>(name, std::move(position));
}

/// ARG = [ ref ] EXPR
std::optional<Argument> Parser::parseArgument() {
    const bool ref{currentToken_.getType() == Token::Type::REF_KW};
    if (ref)
        consumeToken();

    auto argPosition = currentToken_.getPosition();

    auto expr = parseExpression();
    if (!expr) {
        if (ref)
            throw SyntaxException(currentToken_.getPosition(),
                                  "Expected function call argument expression");
        return std::nullopt;
    }

    return Argument{
        .value = std::move(expr), .ref = ref, .position = std::move(argPosition)};
}

Parser::StatementParsers Parser::statementParsers_{
    [](Parser& p) { return p.parseIfStatement(); },
    [](Parser& p) { return p.parseWhileStatement(); },
    [](Parser& p) { return p.parseReturnStatement(); },
    [](Parser& p) { return p.parsePrintStatement(); },
    [](Parser& p) { return p.parseConstVarDef(); },
    [](Parser& p) { return p.parseVoidFunc(); },
    [](Parser& p) { return p.parseDefOrAssignment(); },
    [](Parser& p) { return p.parseBuiltInDef(); },
    [](Parser& p) { return p.parseStructDef(); },
    [](Parser& p) { return p.parseVariantDef(); },
};
