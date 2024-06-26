#ifndef EXPR_INTERPRETER
#define EXPR_INTERPRETER

#include "parse_tree.hpp"
#include "value_obj.hpp"

class Interpreter;

class ExpressionInterpreter : public ExpressionVisitor {
   public:
    ExpressionInterpreter(Interpreter* interpreter);

    /// @brief Returns the result of most recent expression
    ValueHolder getLastResult() const { return std::move(lastResult_); }

    void operator()(const StructInitExpression& expr) const override;
    void operator()(const DisjunctionExpression& expr) const override;
    void operator()(const ConjunctionExpression& expr) const override;
    void operator()(const EqualExpression& expr) const override;
    void operator()(const NotEqualExpression& expr) const override;
    void operator()(const LessThanExpression& expr) const override;
    void operator()(const LessThanOrEqualExpression& expr) const override;
    void operator()(const GreaterThanExpression& expr) const override;
    void operator()(const GreaterThanOrEqualExpression& expr) const override;
    void operator()(const AdditionExpression& expr) const override;
    void operator()(const SubtractionExpression& expr) const override;
    void operator()(const MultiplicationExpression& expr) const override;
    void operator()(const DivisionExpression& expr) const override;
    void operator()(const SignChangeExpression& expr) const override;
    void operator()(const LogicalNegationExpression& expr) const override;
    void operator()(const ConversionExpression& conversionExpr) const override;
    void operator()(const TypeCheckExpression& expr) const override;
    void operator()(const FieldAccessExpression& expr) const override;
    void operator()(const Constant& expr) const override;
    void operator()(const FuncCall& funcCall) const override;
    void operator()(const VariableAccess& expr) const override;

   private:
    ValueObj getExprValue(const Expression& expr) const;
    bool getBoolValue(const Expression& expr) const;

    template <typename Functor>
    void evalLogicalExpr(const BinaryExpression& expr, const Functor& func) const;

    template <typename Functor>
    void evalEqualityExpr(const BinaryExpression& expr, const Functor& func) const;

    template <typename Functor>
    void compareExpr(const BinaryExpression& expr, const Functor& func) const;

    template <typename Functor>
    void evalNumericExpr(const BinaryExpression& expr, const Functor& func) const;

    Interpreter* interpreter_;
    mutable ValueHolder lastResult_;
};

#endif
