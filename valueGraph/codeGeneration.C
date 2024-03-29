#include "functionReverser.h"
#include "valueGraphNode.h"
#include <utilities/utilities.h>
#include <sageBuilder.h>
#include <sageInterface.h>


#define ROSS


namespace Backstroke
{

using namespace std;
using namespace SageBuilder;
using namespace SageInterface;

string getVarNameString(const VarName& var)
{
    string name = var[0]->get_name();
    for (int i = 1, s = var.size(); i < s; ++i)
        name += "_" + var[i]->get_name();  
    return name;
}

//! Build a variable expression from a value node in the value graph.
SgExpression* buildVariable(ValueNode* node)
{
    // I think there may be a bug here, since why I did not use 
    // node->var.getVarRefExp() before?

    SgExpression* var = NULL;
    
    if (var = node->buildExpression())
        return var;

    if (node->isAvailable())
        var = SageInterface::copyExpression(isSgExpression(node->astNode));
    else
    {
        if (node->isStateVar)
            var = node->buildExpression();
        else
            var = buildVarRefExp(getVarNameString(node->getVarName()));
    }
    return var;
}

SgStatement* buildVarDeclaration(ScalarValueNode* newVar, SgExpression* expr)
{
    SgAssignInitializer* init = expr ? buildAssignInitializer(expr) : NULL;
    return buildVariableDeclaration(newVar->var.name[0]->get_name(),
                                    newVar->getType(),
                                    init);
}

SgStatement* buildVarDeclaration(const VarName& newVar, SgExpression* expr)
{
    SgAssignInitializer* init = expr ? buildAssignInitializer(expr) : NULL;
    return buildVariableDeclaration(getVarNameString(newVar),
                                    newVar.back()->get_type(),
                                    init);
}

void instrumentPushFunction(ValueNode* valNode, SgNode* astNode)
{
    //cout << astNode->class_name() << endl;
    
    // Build push function call.
	//SgExprListExp* parameters = buildExprListExp(valNode->var.getVarRefExp());
	//SgExpression* pushFunc = buildFunctionCallExp("push", buildVoidType(), parameters);    
    SgExpression* pushFunc = buildPushFunctionCall(valNode->buildExpression());
    SgStatement* pushFuncStmt = buildExprStatement(pushFunc);
    
    SgStatement* stmt = SageInterface::getEnclosingStatement(astNode);
    SageInterface::insertStatementBefore(stmt, pushFuncStmt); 
}

void instrumentPushFunction(ValueNode* valNode, SgFunctionDefinition* funcDef)
{
    // Build push function call.
	//SgExprListExp* parameters = buildExprListExp(valNode->var.getVarRefExp());
	//SgExpression* pushFunc = buildFunctionCallExp("push", buildVoidType(), parameters);
    SgExpression* pushFunc = buildPushFunctionCall(valNode->buildExpression());
    //return buildExprStatement(pushFunc);

    SgNode* varNode = valNode->astNode;
    if (SgExpression* expr = isSgExpression(varNode))
    {
        // If the target node is an expression, we have to find a proper place
        // to place the push function. Here we insert the push function after its
        // belonged statement.
        SgExpression* commaOp = buildCommaOpExp(pushFunc, copyExpression(expr));
        replaceExpression(expr, commaOp);
    }
    else if (SgInitializedName* initName = isSgInitializedName(varNode))
    {
        // If the target node (def node) is a variable declaration, find this
        // declaration statement first.
        SgNode* stmtNode = initName;
        while (stmtNode && !isSgStatement(stmtNode))
            stmtNode = stmtNode->get_parent();

        SgStatement* pushFuncStmt = buildExprStatement(pushFunc);

        // If this node belongs to event parameters, or is defined outside the function, 
        // add the push function just at the beginning of the forward function.
        // Else, add the push function under the declaration.
        if (isSgClassDefinition(stmtNode->get_parent()) ||
                isSgFunctionParameterList(stmtNode))
        {
            // If this declaration belongs to parameter list.
            prependStatement(pushFuncStmt, funcDef->get_body());
        }
        else
        {
            // Or it is a normal declaration.
            SgVariableDeclaration* varDecl = isSgVariableDeclaration(stmtNode);
            ROSE_ASSERT(varDecl);

            // If this declaration is not in if's condition, while's condition, etc,
            // add the push function just below it.
            if (BackstrokeUtility::isTrueVariableDeclaration(varDecl))
                insertStatementAfter(varDecl, pushFuncStmt);
            else
                ROSE_ASSERT(!"Declaration in conditions is not handled yet!");
        }

        //cout << initName->get_parent()->class_name() << endl;
    }
}


SgExpression* buildPushFunctionCall(SgExpression* para)
{
#ifdef ROSS
    SgExpression* lp = SageBuilder::buildVarRefExp("lp");
    return buildFunctionCallExp("push", buildVoidType(), 
            SageBuilder::buildExprListExp(para, lp));
#else
    return buildFunctionCallExp("push", buildVoidType(), 
            SageBuilder::buildExprListExp(para));
#endif
}

SgExpression* buildStoreFunctionCall(SgExpression* para)
{
#ifdef ROSS
    SgExpression* lp = SageBuilder::buildVarRefExp("lp");
    
    // For a pointer to the class type, we store the dereference of it.
    if (SgArrowExp* arrowExp = isSgArrowExp(para))
    {
        if (SgVarRefExp* varRef = isSgVarRefExp(arrowExp->get_rhs_operand()))
        {
            if (isSgPointerType(varRef->get_type()))
                para = SageBuilder::buildPointerDerefExp(para);
        }
    }
 
    return buildFunctionCallExp("__store__", buildVoidType(), 
            SageBuilder::buildExprListExp(para, lp));
#else
    return buildFunctionCallExp("__store__", buildVoidType(), 
            SageBuilder::buildExprListExp(para));
#endif
}

SgExpression* buildRestoreFunctionCall(SgExpression* para)
{
#ifdef ROSS
    SgExpression* lp = SageBuilder::buildVarRefExp("lp");
    
    // For a pointer to the class type, we store the dereference of it.
    if (SgArrowExp* arrowExp = isSgArrowExp(para))
    {
        if (SgVarRefExp* varRef = isSgVarRefExp(arrowExp->get_rhs_operand()))
        {
            if (isSgPointerType(varRef->get_type()))
                para = SageBuilder::buildPointerDerefExp(para);
        }
    }
    
    
    return buildFunctionCallExp("__restore__", buildVoidType(), 
            SageBuilder::buildExprListExp(para, lp));
#else
    return buildFunctionCallExp("__restore__", buildVoidType(), 
            SageBuilder::buildExprListExp(para));
#endif
}

SgStatement* buildPushStatement(ValueNode* valNode)
{
    return buildExprStatement(buildPushFunctionCall(valNode->buildExpression()));
}

SgStatement* buildPushStatementForPointerType(ValueNode* valNode)
{
#if 0
    SgExpression* var = buildPointerDerefExp(valNode->var.getVarRefExp());
    SgExprListExp* exprList = buildExprListExp(var);
    SgPointerType* ptrType = isSgPointerType(valNode->getType());
    ROSE_ASSERT(ptrType);
    SgType* type = ptrType->get_base_type();
    SgConstructorInitializer* initializer = 
            buildConstructorInitializer(0, exprList, type, 0, 0, 1, 0);
    SgNewExp* newExp = buildNewExp(type, 0, initializer, 0, 0, 0);
#endif
        
    return buildExprStatement(
            buildPushFunctionCall(
                buildCloneFunctionCall(valNode->buildExpression(), valNode->getType())));
}

SgExpression* buildCloneFunctionCall(SgExpression* exp, SgType* type)
{
    SgExprListExp* exprList = buildExprListExp(exp);
    return buildFunctionCallExp("__clone__", type, exprList); 
}

SgExpression* buildDestroyFunctionCall(SgExpression* exp)
{
    SgExprListExp* exprList = buildExprListExp(exp);
    return buildFunctionCallExp("__destroy__", buildVoidType(), exprList); 
}

SgExpression* buildPopFunctionCall(SgType* type)
{
    return buildFunctionCallExp("__pop__< " + get_type_name(type) + " >",
            type, SageBuilder::buildExprListExp());
}

SgStatement* buildPopStatement(SgType* type)
{
    return buildExprStatement(buildPopFunctionCall(type));
}

SgExpression* buildRestorationExp(ValueNode* node)
{
    SgType* type = node->getType();
	SgExpression* popFunc = buildPopFunctionCall(type);
    return buildAssignOp(buildVariable(node), popFunc);
}

SgStatement* buildRestorationStmt(ValueNode* node)
{
    return buildExprStatement(buildRestorationExp(node));
}

#if 0
SgExpression* buildVirtualFunctionCall(const string& funcName, SgType* returnType)
{
    SgExpression* funcCall = buildFunctionCallExp(funcName, returnType);
    return buildArrowOp(buildThis)
        cmtStmt = buildExprStatement(cmtFuncCall);
}
#endif

SgStatement* buildAssignOpertaion(ValueNode* lhs, ValueNode* rhs)
{
    SgExpression* expr;
    // If rhs is NULL, it's an assignment to itself, like a_1 = a;
    if (rhs == NULL)
        expr = lhs->buildExpression();
    else
        expr = buildVariable(rhs);
    
#if 0
    if (isSgInitializedName(lhs->astNode) || lhs->isTemp())
    {
        SgAssignInitializer* assignInit = buildAssignInitializer(expr, expr->get_type());
        return buildVariableDeclaration(lhs->str, lhs->getType(), assignInit);
    }
#endif
    return buildExprStatement(buildAssignOp(buildVariable(lhs), expr));
}

SgStatement* buildOperationStatement(
        ValueNode* result,
        VariantT type,
        ValueNode* lhs,
        ValueNode* rhs)
{
    //SgExpression* resExpr = buildVariable(result);
    SgExpression* lhsExpr = buildVariable(lhs);
    //cout << lhsExpr->unparseToString() << endl;
    SgExpression* rhsExpr = rhs ? buildVariable(rhs) : NULL;
    //if (rhsExpr) cout << rhs << rhsExpr->unparseToString() << endl;
    SgExpression* opExpr  = NULL;

    // Unary expression case.
    if (rhs == NULL)
    {
        switch (type)
        {

#define BUILD_UNARY_OP(suffix) \
case V_Sg##suffix: opExpr = build##suffix(lhsExpr); break;

            BUILD_UNARY_OP(AddressOfOp)
            BUILD_UNARY_OP(BitComplementOp)
            BUILD_UNARY_OP(MinusOp)
            BUILD_UNARY_OP(NotOp)
            BUILD_UNARY_OP(PointerDerefExp)
            BUILD_UNARY_OP(UnaryAddOp)
            BUILD_UNARY_OP(MinusMinusOp)
            BUILD_UNARY_OP(PlusPlusOp)
            BUILD_UNARY_OP(RealPartOp)
            BUILD_UNARY_OP(ImagPartOp)
            BUILD_UNARY_OP(ConjugateOp)
            BUILD_UNARY_OP(VarArgStartOneOperandOp)
            BUILD_UNARY_OP(VarArgEndOp)

#undef BUILD_BINARY_OP

            default:
                break;
        }
    }
    else
    {
        switch (type)
        {

#define BUILD_BINARY_OP(suffix) \
case V_Sg##suffix: opExpr = build##suffix(lhsExpr, rhsExpr); break;

            BUILD_BINARY_OP(AddOp)
            BUILD_BINARY_OP(AndAssignOp)
            BUILD_BINARY_OP(AndOp)
            BUILD_BINARY_OP(ArrowExp)
            BUILD_BINARY_OP(ArrowStarOp)
            BUILD_BINARY_OP(AssignOp)
            BUILD_BINARY_OP(BitAndOp)
            BUILD_BINARY_OP(BitOrOp)
            BUILD_BINARY_OP(BitXorOp)

            BUILD_BINARY_OP(CommaOpExp)
            BUILD_BINARY_OP(ConcatenationOp)
            BUILD_BINARY_OP(DivAssignOp)
            BUILD_BINARY_OP(DivideOp)
            BUILD_BINARY_OP(DotExp)
            BUILD_BINARY_OP(DotStarOp)
            BUILD_BINARY_OP(EqualityOp)

            BUILD_BINARY_OP(ExponentiationOp)
            BUILD_BINARY_OP(GreaterOrEqualOp)
            BUILD_BINARY_OP(GreaterThanOp)
            BUILD_BINARY_OP(IntegerDivideOp)
            BUILD_BINARY_OP(IorAssignOp)

            BUILD_BINARY_OP(LessOrEqualOp)
            BUILD_BINARY_OP(LessThanOp)
            BUILD_BINARY_OP(LshiftAssignOp)
            BUILD_BINARY_OP(LshiftOp)

            BUILD_BINARY_OP(MinusAssignOp)
            BUILD_BINARY_OP(ModAssignOp)
            BUILD_BINARY_OP(ModOp)
            BUILD_BINARY_OP(MultAssignOp)
            BUILD_BINARY_OP(MultiplyOp)

            BUILD_BINARY_OP(NotEqualOp)
            BUILD_BINARY_OP(OrOp)
            BUILD_BINARY_OP(PlusAssignOp)
            BUILD_BINARY_OP(PntrArrRefExp)
            BUILD_BINARY_OP(RshiftAssignOp)

            BUILD_BINARY_OP(RshiftOp)
            BUILD_BINARY_OP(ScopeOp)
            BUILD_BINARY_OP(SubtractOp)
            BUILD_BINARY_OP(XorAssignOp)

            BUILD_BINARY_OP(VarArgCopyOp)
            BUILD_BINARY_OP(VarArgStartOp)

#undef BUILD_BINARY_OP

            default:
                break;
        }
    }

    // For ++ and -- operators, no assignment is needed.
    if (type == V_SgMinusMinusOp || type == V_SgPlusPlusOp)
        return buildExprStatement(opExpr);
    
    ROSE_ASSERT(opExpr);
    return buildExprStatement(buildAssignOp(buildVariable(result), opExpr));
}

} // end of Backstroke
