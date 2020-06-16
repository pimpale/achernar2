#include "token.h"
#include "utils.h"

const char *tk_strKind(tk_Kind val) {
  switch (val) {
  case tk_Eof:
    return "Eof";
  case tk_None:
    return "None";
  case tk_Identifier:
    return "Identifier";
  case tk_Unreachable:
    return "Unreachable";
  case tk_Loop:
    return "Loop";
  case tk_Match:
    return "Match";
  case tk_Val:
    return "Val";
  case tk_Return:
    return "Return";
  case tk_Defer:
    return "Defer";
  case tk_Fn:
    return "Fn";
  case tk_Pat:
    return "Pat";
  case tk_As:
    return "As";
  case tk_Struct:
    return "Struct";
  case tk_Enum:
    return "Enum";
  case tk_Type:
    return "Type";
  case tk_Namespace:
    return "Namespace";
  case tk_Use:
    return "Use";
  case tk_Nil:
    return "Nil";
  case tk_Bool:
    return "Bool";
  case tk_String:
    return "String";
  case tk_Char:
    return "Char";
  case tk_Float:
    return "Float";
  case tk_Int:
    return "Int";
  case tk_Negate:
    return "Negate";
  case tk_Posit:
    return "Posit";
  case tk_Add:
    return "Add";
  case tk_Sub:
    return "Sub";
  case tk_Mul:
    return "Mul";
  case tk_Div:
    return "Div";
  case tk_Mod:
    return "Mod";
  case tk_And:
    return "And";
  case tk_Or:
    return "Or";
  case tk_Not:
    return "Not";
  case tk_CompEqual:
    return "CompEqual";
  case tk_CompNotEqual:
    return "CompNotEqual";
  case tk_CompLess:
    return "CompLess";
  case tk_CompLessEqual:
    return "CompLessEqual";
  case tk_CompGreater:
    return "CompGreater";
  case tk_CompGreaterEqual:
    return "CompGreaterEqual";
  case tk_Ref:
    return "Ref";
  case tk_Deref:
    return "Deref";
  case tk_Define:
    return "Define";
  case tk_Assign:
    return "Assign";
  case tk_AssignAdd:
    return "AssignAdd";
  case tk_AssignSub:
    return "AssignSub";
  case tk_AssignMul:
    return "AssignMul";
  case tk_AssignDiv:
    return "AssignDiv";
  case tk_AssignMod:
    return "AssignMod";
  case tk_Pipe:
    return "Pipe";
  case tk_Arrow:
    return "Arrow";
  case tk_ScopeResolution:
    return "ScopeResolution";
  case tk_Tuple:
    return "Tuple";
  case tk_Union:
    return "Union";
  case tk_ParenLeft:
    return "ParenLeft";
  case tk_ParenRight:
    return "ParenRight";
  case tk_BracketLeft:
    return "BracketLeft";
  case tk_BracketRight:
    return "BracketRight";
  case tk_BraceLeft:
    return "BraceLeft";
  case tk_BraceRight:
    return "BraceRight";
  case tk_FieldAccess:
    return "FieldAccess";
  case tk_Colon:
    return "Colon";
  case tk_Semicolon:
    return "Semicolon";
  case tk_Underscore:
    return "Underscore";
  case tk_Backtick:
    return "Backtick";
  case tk_Rest:
    return "Rest";
  case tk_Dollar:
    return "Dollar";
  case tk_Label:
    return "Label";
  case tk_Macro:
    return "Macro";
  case tk_Comment:
    return "Comment";
  }
  PANIC();
}

