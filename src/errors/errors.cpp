#include <limits>

#include "interpreter_errors.hpp"
#include "lexer_errors.hpp"
#include "magic_enum/magic_enum.hpp"

BaseException::BaseException(const Position& position, const std::string& message)
    : std::runtime_error(" at " + std::to_string(position.line) + ':'
                         + std::to_string(position.column) + '\n' + message),
      position_(position) {}

std::string BaseException::getName() const {
    return typeid(*this).name();
}

std::string BaseException::describe() const {
    return getName() + what();
}

InvalidToken::InvalidToken(const Position& position, char c)
    : BaseException(position,
                    "Unknown token starting with '" + std::string(1, c) + '\'') {}

NotTerminatedStrConst::NotTerminatedStrConst(const Position& position)
    : BaseException(position, "Encountered end of file while processing str literal") {}

NonEscapableChar::NonEscapableChar(const Position& position, char c)
    : BaseException(position,
                    '\'' + std::string(1, c) + "' cannot be escaped with '\\'") {}

NumericOverflow::NumericOverflow(const Position& position, Integral value, Integral digit)
    : BaseException(position, "Detected overflow while constructing numeric literal\n"
                                  + std::to_string(value) + " * 10 + "
                                  + std::to_string(digit) + " > "
                                  + std::to_string(std::numeric_limits<Integral>::max())
                                  + " which is maximum value") {}

InvalidFloat::InvalidFloat(const Position& position)
    : BaseException(position, "Expected digit after '.' in float literal") {}

struct TypeToString {
    std::string operator()(BuiltInType type) {
        return std::string(magic_enum::enum_name(type));
    }
    std::string operator()(const std::string& type) { return type; }
    std::string operator()(const VoidType&) { return "VOID"; }
};

SymbolNotFound::SymbolNotFound(const Position& position, std::string type,
                               std::string symbol)
    : BaseException{position, type + " " + symbol + " not found"},
      type_{type},
      symbol_{symbol} {}

SymbolNotFound::SymbolNotFound(const Position& position, const SymbolNotFound& e)
    : SymbolNotFound{position, e.type_, e.symbol_} {}

TypeMismatch::TypeMismatch(const Position& position, Type expected, Type actual)
    : BaseException{position, "Expected: " + std::visit(TypeToString(), expected)
                                  + "\nActual: " + std::visit(TypeToString(), actual)},
      expected_{expected},
      actual_{actual} {}

TypeMismatch::TypeMismatch(const Position& position, const TypeMismatch& e)
    : TypeMismatch{position, e.expected_, e.actual_} {}

ReturnTypeMismatch::ReturnTypeMismatch(const Position& position, ReturnType expected,
                                       ReturnType actual)
    : BaseException{position, "Expected: " + std::visit(TypeToString(), expected)
                                  + "\nActual: " + std::visit(TypeToString(), actual)},
      expected_{expected},
      actual_{actual} {}

ReturnTypeMismatch::ReturnTypeMismatch(const Position& position,
                                       const ReturnTypeMismatch& e)
    : ReturnTypeMismatch{position, e.expected_, e.actual_} {}

InvalidFieldCount::InvalidFieldCount(const Position& position, std::size_t expected,
                                     std::size_t actual)
    : BaseException{position, "Expected " + std::to_string(expected) + " fields but "
                                  + std::to_string(actual) + " were given"},
      expected_{expected},
      actual_{actual} {}

InvalidFieldCount::InvalidFieldCount(const Position& position, const InvalidFieldCount& e)
    : InvalidFieldCount{position, e.expected_, e.actual_} {}

InvalidField::InvalidField(const Position& position, std::string_view fieldName)
    : BaseException{position, "Invalid struct's field name " + std::string(fieldName)},
      fieldName_{fieldName} {}

InvalidField::InvalidField(const Position& position, const InvalidField& e)
    : InvalidField{position, e.fieldName_} {}

Redefinition::Redefinition(const Position& position, std::string type, std::string name)
    : BaseException{position, "Redefinition of " + name + " " + type}, name_{name} {}

struct ValueToString {
    std::string operator()(Integral) const { return "INT"; }
    std::string operator()(Floating) const { return "FLOAT"; }
    std::string operator()(bool) const { return "BOOL"; }
    std::string operator()(const std::string&) const { return "STR"; }
    std::string operator()(const StructObj&) const { return "Anonymous struct"; }
    std::string operator()(const NamedStructObj& s) const {
        return "Struct " + s.structDef->name;
    }
    std::string operator()(const VariantObj& v) const {
        return "Variant " + v.variantDef->name;
    }
};

InvalidTypeConversion::InvalidTypeConversion(const Position& position,
                                             ValueObj::Value from, Type to)
    : BaseException{position, "Cannot convert from " + std::visit(ValueToString(), from)
                                  + " to " + std::visit(TypeToString(), to)},
      from_{std::move(from)},
      to_{to} {}

InvalidTypeConversion::InvalidTypeConversion(const Position& position,
                                             InvalidTypeConversion e)
    : InvalidTypeConversion{position, std::move(e.from_), e.to_} {}
