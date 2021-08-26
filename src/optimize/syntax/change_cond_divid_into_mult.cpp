#include "../../front/syntax/syntax_analyze.h"
#include <memory>
#include <climits>

static shared_ptr<UnaryExpNode> dividedUnaryExp;
static string op;
static bool isPlus;
static shared_ptr<MulExpNode> leftMulExp;
static shared_ptr<MulExpNode> rightMulExp;
long long leftValue;
long long rightValue;

bool isCondChangeable(const shared_ptr<CondNode> &condNode) {
    auto lOrExp = condNode->lOrExp;
    if (lOrExp->lOrExp) {
        return false;
    }
    auto lAndExp = lOrExp->lAndExp;
    if (lAndExp->lAndExp) {
        return false;
    }
    auto eqExp = lAndExp->eqExp;
    if (eqExp->eqExp) {
        return false;
    }
    auto relExp = eqExp->relExp;
    op = relExp->op;
    if (relExp->relExp && relExp->addExp) {
        if (relExp->relExp->relExp) {
            return false;
        }
        auto addExp = relExp->relExp->addExp;
        if (addExp->addExp) {
            return false;
        }
        auto mulExp = addExp->mulExp;
        leftMulExp = mulExp;
        if (mulExp->mulExp && mulExp->unaryExp) {
            if (mulExp->op != "/") {
                return false;
            }
            auto unaryExp = mulExp->unaryExp;
            dividedUnaryExp = unaryExp;
            bool isPlusUnary = true;
            while (unaryExp->type == UNARY_DEEP) {
                if (unaryExp->op == "-") {
                    isPlusUnary = !isPlusUnary;
                }
                unaryExp = unaryExp->unaryExp;
            }
            if (unaryExp->type != UNARY_PRIMARY) {
                return false;
            }
            auto primaryExp = unaryExp->primaryExp;
            if (primaryExp->type == PrimaryExpType::PRIMARY_NUMBER) {
                isPlus = (primaryExp->number > 0);
                isPlus = isPlusUnary == isPlus;
                leftValue = isPlusUnary ? primaryExp->number : -primaryExp->number;
            } else if (primaryExp->type == PrimaryExpType::PRIMARY_L_VAL) {
                auto lVal = primaryExp->lVal;
                if (lVal->ident->ident->symbolType != SymbolType::CONST_VAR) {
                    return false;
                }
                isPlus = (s_p_c<ConstInitValValNode>(lVal->ident->ident->constInitVal)->value > 0);
                isPlus = isPlusUnary == isPlus;
                leftValue = isPlusUnary ? s_p_c<ConstInitValValNode>(lVal->ident->ident->constInitVal)->value
                                        : -s_p_c<ConstInitValValNode>(lVal->ident->ident->constInitVal)->value;
            } else {
                return false;
            }
        } else {
            return false;
        }
        addExp = relExp->addExp;
        if (addExp->addExp) {
            return false;
        }
        mulExp = addExp->mulExp;
        rightMulExp = mulExp;
        if (mulExp->mulExp) {
            return false;
        }
        auto unaryExp = mulExp->unaryExp;
        bool isPlusUnary = true;
        while (unaryExp->type == UNARY_DEEP) {
            if (unaryExp->op == "-") {
                isPlusUnary = !isPlusUnary;
            }
            unaryExp = unaryExp->unaryExp;
        }
        if (unaryExp->type != UNARY_PRIMARY) {
            return false;
        }
        auto primaryExp = unaryExp->primaryExp;
        if (primaryExp->type == PrimaryExpType::PRIMARY_NUMBER) {
            rightValue = isPlusUnary ? primaryExp->number : -primaryExp->number;
        } else if (primaryExp->type == PrimaryExpType::PRIMARY_L_VAL) {
            auto lVal = primaryExp->lVal;
            if (lVal->ident->ident->symbolType != SymbolType::CONST_VAR) {
                return false;
            }
            rightValue = isPlusUnary ? s_p_c<ConstInitValValNode>(lVal->ident->ident->constInitVal)->value
                                     : -s_p_c<ConstInitValValNode>(lVal->ident->ident->constInitVal)->value;
        } else {
            return false;
        }
    } else {
        return false;
    }
    if ((rightValue * leftValue > INT_MAX) || (rightValue * leftValue < INT_MIN)) {
        return false;
    }
    return true;
}

void makeDividedUnaryIntoPlusOneUnary() {
    int one = 1;
    string oneOp = "+";
    auto onePrimaryExp = make_shared<PrimaryExpNode>(PrimaryExpNode::numberExp(one));
    auto oneUnaryExp = make_shared<UnaryExpNode>(UnaryExpNode::primaryUnaryExp(onePrimaryExp, oneOp));
    auto oneMulExp = make_shared<MulExpNode>(oneUnaryExp);
    auto oneAddExp = make_shared<AddExpNode>(oneMulExp);
    auto dividedMulExp = make_shared<MulExpNode>(dividedUnaryExp);
    oneAddExp = make_shared<AddExpNode>(dividedMulExp, oneAddExp, oneOp);
    shared_ptr<ExpNode> oneExpNode = oneAddExp;
    auto addOnePrimaryExp = make_shared<PrimaryExpNode>(PrimaryExpNode::parentExp(oneExpNode));
    dividedUnaryExp = make_shared<UnaryExpNode>(UnaryExpNode::primaryUnaryExp(addOnePrimaryExp, oneOp));
}

void changeCondDivideIntoMul(shared_ptr<CondNode> &condNode) {
    if (!isCondChangeable(condNode)) {
        return;
    }
    leftMulExp->unaryExp = leftMulExp->mulExp->unaryExp;
    leftMulExp->mulExp = shared_ptr<MulExpNode>(nullptr);
    leftMulExp->op = "";
    rightMulExp->mulExp = make_shared<MulExpNode>(rightMulExp->unaryExp);
    rightMulExp->op = "*";
    if (op == ">=") {
        if (!isPlus) {
            condNode->lOrExp->lAndExp->eqExp->relExp->op = "<=";
            makeDividedUnaryIntoPlusOneUnary();
        }
    } else if (op == ">") {
        if (!isPlus) {
            condNode->lOrExp->lAndExp->eqExp->relExp->op = "<";
        } else {
            makeDividedUnaryIntoPlusOneUnary();
        }
    } else if (op == "<") {
        if (!isPlus) {
            condNode->lOrExp->lAndExp->eqExp->relExp->op = ">";
            makeDividedUnaryIntoPlusOneUnary();
        }
    } else if (op == "<=") {
        if (!isPlus) {
            condNode->lOrExp->lAndExp->eqExp->relExp->op = ">=";
        } else {
            makeDividedUnaryIntoPlusOneUnary();
        }
    }
    rightMulExp->unaryExp = dividedUnaryExp;
}
