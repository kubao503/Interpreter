#ifndef EXPR_IR_GENERATOR_H
#define EXPR_IR_GENERATOR_H

#include "parse_tree.hpp"

class ExprIRGenerator : public ExpressionVisitor {
    void operator()(const StructInitExpression& expr) const override;
    void operator()(const DisjunctionExpression& disjunction) const override;
    void operator()(const ConjunctionExpression& conjunction) const override;
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
    void operator()(const ConversionExpression& expr) const override;
    void operator()(const TypeCheckExpression& expr) const override;
    void operator()(const FieldAccessExpression& expr) const override;
    void operator()(const Constant& expr) const override;
    void operator()(const FuncCall& expr) const override;
    void operator()(const VariableAccess& expr) const override;
};

#endif  // EXPR_IR_GENERATOR_H