#include "printer.hpp"

#include "magic_enum/magic_enum.hpp"

std::ostream& operator<<(std::ostream& stream, const Program& program) {
    for (const auto& stmt : program.statements) {
        stream << std::visit(StatementPrinter(), stmt) << '\n';
    }
    return stream;
}

ExpressionPrinter BasePrinter::getSubExprPrinter() const {
    return ExpressionPrinter(indent_ + indentWidth_);
}

StatementPrinter BasePrinter::getSubStmtPrinter() const {
    return StatementPrinter(indent_ + indentWidth_);
}

std::string ExpressionPrinter::printBinaryExpression(const auto& expression) const {
    return std::visit(getSubExprPrinter(), expression->lhs) + '\n'
           + std::visit(getSubExprPrinter(), expression->rhs);
}

std::string ExpressionPrinter::operator()(const StructInitExpression& expr) const {
    std::string output = getPrefix() + "StructInitExpression";
    for (const auto& expr : expr.exprs)
        output += '\n' + std::visit(getSubExprPrinter(), expr);
    return output;
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<DisjunctionExpression>& disjunction) const {
    return getPrefix() + "DisjunctionExpression\n" + printBinaryExpression(disjunction);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<ConjunctionExpression>& conjunction) const {
    return getPrefix() + "ConjunctionExpression\n" + printBinaryExpression(conjunction);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<EqualExpression>& expr) const {
    return getPrefix() + "EqualExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<NotEqualExpression>& expr) const {
    return getPrefix() + "NotEqualExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<LessThanExpression>& expr) const {
    return getPrefix() + "LessThanExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<LessThanOrEqualExpression>& expr) const {
    return getPrefix() + "LessThanOrEqualExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<GreaterThanExpression>& expr) const {
    return getPrefix() + "GreaterThanExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<GreaterThanOrEqualExpression>& expr) const {
    return getPrefix() + "GreaterThanOrEqualExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<AdditionExpression>& expr) const {
    return getPrefix() + "AdditionExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<SubtractionExpression>& expr) const {
    return getPrefix() + "SubtractionExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<MultiplicationExpression>& expr) const {
    return getPrefix() + "MultiplicationExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<DivisionExpression>& expr) const {
    return getPrefix() + "DivisionExpression\n" + printBinaryExpression(expr);
}

std::string ExpressionPrinter::operator()(
    const std::unique_ptr<FieldAccessExpression>& expr) const {
    return getPrefix() + "FieldAccessExpression\n"
           + std::visit(getSubExprPrinter(), expr->expr) + '\n' + getPrefix()
           + "  field: " + expr->field;
}

std::string ExpressionPrinter::operator()(const VariableAccess& expr) const {
    return getPrefix() + "VariableAccess " + expr.name;
}

std::string ExpressionPrinter::operator()(const Constant& expr) const {
    return getPrefix() + "Constant: " + std::visit(ValuePrinter(), expr.value);
}

std::string ExpressionPrinter::operator()(const auto& expr) const {
    return getPrefix() + typeid(expr).name();
}

std::string StatementPrinter::printIfOrWhileStatement(
    const IfOrWhileStatement& ifOrWhile) const {
    std::string output = getPrefix() + "└condition:\n"
                         + std::visit(getSubExprPrinter(), ifOrWhile.condition) + '\n'
                         + getPrefix() + "└statements {";
    for (const auto& stmt : ifOrWhile.statements)
        output += '\n' + std::visit(getSubStmtPrinter(), stmt);
    return output + '\n' + getPrefix() + '}';
}

std::string StatementPrinter::operator()(const IfStatement& ifStatement) const {
    return getPrefix() + "IfStatement\n" + printIfOrWhileStatement(ifStatement);
}

std::string StatementPrinter::operator()(const WhileStatement& whileStatement) const {
    return getPrefix() + "WhileStatement\n" + printIfOrWhileStatement(whileStatement);
}

std::string StatementPrinter::printReturnOrPrintStatement(
    const ReturnOrPrintStatement& stmt) const {
    return stmt.expression ? std::visit(getSubExprPrinter(), *stmt.expression) : "";
}

std::string StatementPrinter::operator()(const ReturnStatement& stmt) const {
    return getPrefix() + "ReturnStatement\n" + printReturnOrPrintStatement(stmt);
}

std::string StatementPrinter::operator()(const PrintStatement& stmt) const {
    return getPrefix() + "PrintStatement\n" + printReturnOrPrintStatement(stmt);
}

std::string StatementPrinter::operator()(const FuncDef& funcDef) const {
    std::string output = getPrefix() + "FuncDef " + funcDef.getName();
    for (const auto& param : funcDef.getParameters()) output += ' ' + param.name + ',';
    output += " {";
    for (const auto& stmt : funcDef.getStatements())
        output += '\n' + std::visit(getSubStmtPrinter(), stmt);
    return output + '\n' + getPrefix() + '}';
}

std::string StatementPrinter::operator()(const Assignment& stmt) const {
    return getPrefix() + "Assignment\n"
           + std::visit(LValuePrinter(indent_ + indentWidth_), stmt.lhs) + '\n'
           + std::visit(getSubExprPrinter(), stmt.rhs);
}

std::string StatementPrinter::operator()(const VarDef& stmt) const {
    return getPrefix() + "VarDef\n" + getPrefix()
           + "  is const: " + std::to_string(stmt.isConst) + '\n' + getPrefix()
           + "  type: " + std::visit(TypePrinter(indent_ + indentWidth_), stmt.type)
           + '\n' + getPrefix() + "  name: " + stmt.name + '\n' + getPrefix()
           + "  value:\n" + std::visit(getSubExprPrinter(), stmt.expression);
}

std::string StatementPrinter::operator()(const FuncCall& stmt) const {
    std::string output = getPrefix() + "FuncCall\n" + getPrefix() + "  name: " + stmt.name
                         + '\n' + getPrefix() + "  args: ";
    for (const auto& arg : stmt.arguments)
        output += '\n' + std::visit(getSubExprPrinter(), arg.value);
    return output;
}

std::string StatementPrinter::operator()(const StructDef& stmt) const {
    std::string output =
        getPrefix() + "StructDef " + stmt.name + '\n' + getPrefix() + "fields: ";
    for (const auto& field : stmt.fields)
        output += '\n' + getPrefix() + "  "
                  + std::visit(TypePrinter(indent_ + indentWidth_), field.type) + ' '
                  + field.name;
    return output;
}

std::string StatementPrinter::operator()(const VariantDef& stmt) const {
    std::string output =
        getPrefix() + "VariantDef " + stmt.name + '\n' + getPrefix() + "types: ";
    for (const auto& type : stmt.types)
        output += '\n' + getPrefix() + "  "
                  + std::visit(TypePrinter(indent_ + indentWidth_), type);
    return output;
}

std::string StatementPrinter::operator()(const auto& stmt) const {
    return getPrefix() + typeid(stmt).name();
}

std::string LValuePrinter::operator()(const std::unique_ptr<FieldAccess>& lvalue) const {
    return getPrefix() + "FieldAcces\n"
           + std::visit(LValuePrinter(indent_ + indentWidth_), lvalue->container) + '\n'
           + getPrefix() + "  field: " + lvalue->field;
}

std::string LValuePrinter::operator()(const std::string& lvalue) const {
    return getPrefix() + "variable: " + lvalue;
}

std::string TypePrinter::operator()(const std::string& type) const {
    return type;
}

std::string TypePrinter::operator()(BuiltInType type) const {
    return std::string(magic_enum::enum_name(type));
}

std::string ValuePrinter::operator()(const std::monostate&) const {
    return "";
}

std::string ValuePrinter::operator()(const std::string& type) const {
    return type;
}

std::string ValuePrinter::operator()(const auto& type) const {
    return std::to_string(type);
}
