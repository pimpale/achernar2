from parsimonious.grammar import Grammar

grammar = Grammar(
    """

Root = Spacing Stmnt* !.

# --- STATEMENTS ---

Stmnt = Metadata ( UseStmnt / ModStmnt / ValDeclStmnt / TypeDeclStmnt / ValExpr )


UseStmnt = KeywordUse ModReference
ModStmnt = KeywordMod ModBinding LBrace Stmnt* RBrace
ValDeclStmnt = KeywordVal LBracket Binding* RBracket PatExpr Define ValExpr
TypeDeclStmnt = KeywordType Binding Define TypeExpr

# --- PATTERN EXPR ---

PatExpr = L4PatExpr

L4PatExpr 
    = ( L3PatExpr Metadata ( KeywordOr / KeywordAnd ) L4PatExpr)
     / L3PatExpr 

L3PatExpr  
    = ( L2PatExpr Metadata ( OpEnum / OpTuple / OpIntersection ) L3PatExpr) 

L2PatExpr 
    = (L1PatExpr Metadata ( OpRef / OpDeref )
     / L1PatExpr 

L1PatExpr  
    = Metadata ( TypeRestrictionPatExpr
                / ValRestrictionPatExpr
                / BlockPatExpr 
                / StructPatExpr
                / MacroExpr )

TypeRestrictionPatExpr
    = ( Colon TypeExpr) 
     / ( Binding Colon TypeExpr)

ValRestrictionPatExpr 
    = ( OpsCmp L1ValExpr )
     / ( Binding OpsCmp L1ValExpr)

BlockPatExpr = LBrace PatExpr RBrace

StructPatExpr = KeywordStruct LBrace FieldPatExpr RBrace

# --- TYPE EXPR ---

TypeExpr = L3TypeExpr

L3TypeExpr 
    = ( L2TypeExpr Metadata ( OpEnum / OpTuple / OpIntersection ) L3TypeExpr) 
     / L2TypeExpr   

L2TypeExpr 
    = (L1TypeExpr Metadata ( TypefnCallTypeExprSuffix 
                            / FieldAccessTypeExprSuffix 
                            / OpRef 
                            / OpDeref )
     / L1TypeExpr 

TypefnCallTypeExprSuffix = LBracket TypeExpr* RBracket

FieldAccessTypeExprSuffix = OpFieldAccess FieldReference

L1TypeExpr = Metadata ( FnTypeExpr 
                       / BlockTypeExpr
                       / RefTypeExpr
                       / TypefnTypeExpr
                       / StructTypeExpr
                       / EnumTypeExpr
                       / KeywordNil 
                       / KeywordNever
                       / MacroExpr
                       )

FnTypeExpr = KeywordFn LParen TypeExpr* RParen TypeExpr

BlockTypeExpr = LBrace TypeExpr RBrace

RefTypeExpr = Reference 

TypefnTypeExpr = KeywordTypefn Binding? LParen Binding* RParen Arrow TypeExpr

StructTypeExpr = KeywordStruct LBrace FieldExpr* RBrace

EnumTypeExpr = KeywordEnum LBrace FieldExpr* RBrace

# --- VALUE EXPR ---

ValExpr = L9ValExpr

L9ValExpr 
     = ( L8ValExpr Metadata ( OpAssign
                             / OpAddAssign
                             / OpSubAssign
                             / OpMulAssign
                             / OpDivAssign
                             / OpRemainderAssign 
                             ) L9ValExpr)
     / L8ValExpr 

L8ValExpr 
    = ( L7ValExpr Metadata ( KeywordOr / KeywordAnd ) L8ValExpr)
     / L7ValExpr 

L7ValExpr 
    = ( L6ValExpr Metadata ( OpEnum / OpTuple / OpIntersection ) L7ValExpr)
     / L6ValExpr 

L6ValExpr 
     = (L5ValExpr Metadata OpsCmp L6ValExpr )
      / L5ValExpr 

L5ValExpr 
    = (L4ValExpr Metadata ( OpSub / OpAdd ) L5ValExpr)
     / L4ValExpr 

L4ValExpr 
    = (L3ValExpr Metadata ( OpMul / OpDiv / OpRemainder) L4ValExpr)
     / L3ValExpr 

L3ValExpr 
    = ( Metadata ( KeywordNot ) L2ValExpr )
     / L2ValExpr 

L2ValExpr 
    = ( L1ValExpr Metadata (
         MatchValExprSuffix
       / FieldAccessValExprSuffix
       / FnCallValExprSuffix
       / FnPipeCallValExprSuffix
       / AsValExprSuffix 
       / OpRef
       / OpDeref )
     / L1ValExpr 

MatchValExprSuffix = KeywordMatch LBrace MatchCaseExpr* RBrace 

MatchCaseExpr = Metadata (MacroExpr / (KeywordPat PatExpr Arrow ValExpr))

FieldAccessValExprSuffix = OpFieldAccess FieldReference

FnCallValExprSuffix = LParen ValExpr* RParen

FnPipeCallValExprSuffix  = OpPipe FnCallValExprSuffix

AsValExprSuffix = KeywordAs TypeExpr

L1ValExpr = Metadata ( FnValExpr 
                      / LoopValExpr 
                      / RetValExpr 
                      / RefValExpr 
                      / BlockValExpr 
                      / KeywordNil 
                      / MacroExpr 
                      / StringLiteral
                      / CharLiteral
                      / IntLiteral
                      / FloatLiteral
                      / StructLiteral
                      )


FnValExpr = KeywordFn Binding? LParen ( ValExpr )* RParen TypeExpr Yields ValExpr

LoopValExpr = KeywordLoop LabelBinding? ValExpr

RetValExpr = KeywordRet LabelReference ValExpr

RefValExpr = Reference GenericResolutionParams?

BlockValExpr = LBrace LabelBinding? Statement* RBrace

StructLiteral = KeywordStruct LBrace FieldLiteralExpr* RBrace

# --- STRUCT FIELDS ---

FieldReference = Identifier
FieldBinding = Identifier

FieldLiteralExpr = Metadata ( (FieldBinding Define ValExpr) / MacroExpr )
FieldExpr = Metadata ( (FieldBinding Colon TypeExpr) / MacroExpr )
FieldPatExpr = Metadata ( (FieldReference Arrow PatExpr) / MacroExpr )

# --- COMMENTS + ATTRIBUTES ---

Metadata = ( Comment / Attribute )*

Comment 
    = WordComment
     / LineComment
     / MultilineComment

WordComment
    = "##" ~"[^\n]*" Spacing

LineComment
    = "#" ~"[A-Za-z_][A-Za-z0-9_]" Spacing 

NotMultiLineComment 
    = (!"#{" !"}#" .) 

MultilineComment 
    = "#{" (NotMultiLineComment / MultilineComment)* "}#" Spacing

Attribute
    = WordAttribute
     / LineAttribute
     / MultilineAttribute

WordAttrbute
    = "$$" ~"[^\n]*" Spacing

LineAttrbute
    = "$" ~"[A-Za-z_][A-Za-z0-9_]" Spacing 

NotMultiLineAttrbute 
    = (!"${" !"}$" .) 

MultilineAttrbute 
    = "${" (NotMultiLineAttrbute / MultilineAttrbute)* "}$" Spacing


# --- MACROS ---

MacroExpr = MacroIdentifier LParen RParen

# --- LABELS + REFERENCES ---

Label = "'" Identifier

LabelReference = Label 
LabelBinding = Label

ModBinding = Identifier
ModReference = ( Identifier OpModModAccess Spacing )* Identifier

Reference = (ModReference OpConcreteModAccess)? Identifier
Binding = Identifier

#EDIT
Identifier = !AnyKeyword !OpUnderscore [A-Za-z_] [A-Za-z0-9_]* Spacing
MacroIdentifier = !OpUnderscore [A-Za-z_][A-Za-z0-9_]*"!" Spacing

# --- LITERALS ---

hex = ~"[0-9a-fA-F]"
hex_ = ('_'/hex)
dec = ~"[0-9]"
dec_ = ('_'/dec)

dec_int = dec (dec_* dec)?
hex_int = hex (hex_* dec)?

FloatLiteral
    =  ~"[-+]?" "0x" hex_* hex "." hex_int (~"[pP]" ~"[-+]?" hex_int)? Spacing
     /  ~"[-+]?"      dec_int   "." dec_int ~"([eE]" ~"[-+]?" dec_int)? Spacing
     /  ~"[-+]?" "0x" hex_* hex "."? ~"[pP]" ~"[-+]?" hex_int Spacing
     /  ~"[-+]?"      dec_int   "."? ~"[eE]" ~"[-+]?" dec_int Spacing

IntLiteral
    = ~"[-+]?" "0b" ~"[_01]*"  ~"[01]"  Spacing
     / ~"[-+]?" "0o" ~"[_0-7]*" ~"[0-7]" Spacing
     / ~"[-+]?" "0x" hex_* hex Spacing
     / ~"[-+]?"      dec_int   Spacing

CharUnit
    = "\\x" hex hex
     / "\\" ~"[nr\\t'"]"
     / ~"[^\\'\n]"

CharLiteral = "'" CharUnit "'" Spacing

StringUnit
    = char_escape
     / "\\u{" hex hex hex hex "}"
     / ~"[^\\"\n]"

SingleStringLiteral 
    = "\"" StringUnit* "\"" Spacing

StringLiteral
    = SingleStringLiteral SingleStringLiteral*

# --- Symbols + Operators---

OpModModAccess          = Slash
OpDiv                   = Slash

OpCmp = OpEqual 
       / OpNotEqual 
       / OpLessEqual 
       / OpLess 
       / OpGreaterEqual 
       / OpGreater

OpUnderscore            = "_"  ~"![A-Za-z_]" Spacing
OpDeref                 = "@"                Spacing
OpRef                   = "&"                Spacing
OpMul                   = "*"  ~"![=]"       Spacing
OpMulAssign             = "*="               Spacing
Colon                   = ":"  ~"![:=]"      Spacing
OpConcreteModAccess     = "::"               Spacing
OpSemi                  = ";"                Spacing
OpTuple                 = ","                Spacing
OpFieldAccess           = "."                Spacing
OpAssign                = "="  ~"![>=]"      Spacing
OpEqual                 = "=="               Spacing
Define                  = ":="               Spacing
Arrow                   = "=>"               Spacing
OpNotEqual              = "!="               Spacing
OpLess                  = "<"  ~"![=]"       Spacing
OpLessEqual             = "<="               Spacing
LBrace                  = "{"                Spacing
LBracket                = "["  ~"![*]"       Spacing
LParen                  = "("                Spacing
OpSub                   = "-"  ~"![=>]"      Spacing
OpSubAssign             = "-="               Spacing
OpPipe                  = "->"               Spacing
OpRemainder             = "%"  ~"![=]"       Spacing
OpRemainderAssign       = "%="               Spacing
OpEnum                  = "|"  ~"![|]"       Spacing
OpIntersection          = "||"               Spacing
OpAdd                   = "+"  ~"![+=]"      Spacing
OpAddAssign             = "+="               Spacing
OpGreater               = ">"  ~"![=]"       Spacing
OpGreaterEqual          = ">="               Spacing
RBrace                  = "}"                Spacing
RBracket                = "]"                Spacing
RParen                  = ")"                Spacing
Slash                   = "/"  ~"![=]"       Spacing
OpDivAssign             = "/="               Spacing

# --- KEYWORDS ---

WordEnd = ~"![a-zA-Z0-9_]" Spacing

KeywordLoop      = "loop"     WordEnd
KeywordMatch     = "match"    WordEnd
KeywordPat       = "pat"      WordEnd
KeywordVal       = "val"      WordEnd
KeywordRet       = "ret"      WordEnd
KeywordDefer     = "defer"    WordEnd
KeywordFn        = "fn"       WordEnd
KeywordPat       = "pat"      WordEnd
KeywordAs        = "as"       WordEnd
KeywordType      = "type"     WordEnd
KeywordTypefn    = "typefn"   WordEnd
KeywordStruct    = "struct"   WordEnd
KeywordEnum      = "enum"     WordEnd
KeywordDyn       = "dyn"      WordEnd
KeywordMod       = "mod"      WordEnd
KeywordUse       = "use"      WordEnd
KeywordTrue      = "true"     WordEnd
KeywordFalse     = "false"    WordEnd
KeywordAnd       = "and"      WordEnd
KeywordOr        = "or"       WordEnd
KeywordNot       = "not"      WordEnd
KeywordNil       = "nil"      WordEnd
KeywordNever     = "never"    WordEnd

AnyKeyword =   
KeywordLoop      \
KeywordMatch     \
KeywordPat       \
KeywordVal       \
KeywordRet       \
KeywordDefer     \
KeywordFn        \
KeywordPat       \
KeywordAs        \
KeywordType      \
KeywordTypefn    \
KeywordStruct    \
KeywordEnum      \
KeywordDyn       \
KeywordMod       \
KeywordUse       \
KeywordTrue      \
KeywordFalse     \
KeywordAnd       \
KeywordOr        \
KeywordNot       \
KeywordNil       \
KeywordNever     

# --- WHITESPACE ---

Spacing = ~"[ \t\n]*"



    
    """)

print(grammar.parse('test code'))

