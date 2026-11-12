#include "syntax.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "ILexer.h"
#include "LexerModule.h"
#include "SciLexer.h"
#include "Scintilla.h"
#include "theme.h"
#include "util.h"

extern const Lexilla::LexerModule lmCPP;
extern const Lexilla::LexerModule lmPython;
extern const Lexilla::LexerModule lmJSON;
extern const Lexilla::LexerModule lmHTML;
extern const Lexilla::LexerModule lmCss;
extern const Lexilla::LexerModule lmMarkdown;
extern const Lexilla::LexerModule lmBash;
extern const Lexilla::LexerModule lmSQL;
extern const Lexilla::LexerModule lmRust;
extern const Lexilla::LexerModule lmRuby;
extern const Lexilla::LexerModule lmPerl;
extern const Lexilla::LexerModule lmLua;
extern const Lexilla::LexerModule lmYAML;
extern const Lexilla::LexerModule lmTOML;
extern const Lexilla::LexerModule lmConf;
extern const Lexilla::LexerModule lmMake;
extern const Lexilla::LexerModule lmBatch;
extern const Lexilla::LexerModule lmPowerShell;
extern const Lexilla::LexerModule lmDiff;
extern const Lexilla::LexerModule lmCmake;
extern const Lexilla::LexerModule lmLatex;
extern const Lexilla::LexerModule lmR;
extern const Lexilla::LexerModule lmHaskell;
extern const Lexilla::LexerModule lmDart;
extern const Lexilla::LexerModule lmZig;
extern const Lexilla::LexerModule lmNix;
extern const Lexilla::LexerModule lmVB;
extern const Lexilla::LexerModule lmProps;
extern const Lexilla::LexerModule lmAsm;

namespace syntax {
namespace {

sptr_t S(HWND sci, unsigned int message, uptr_t wParam = 0, sptr_t lParam = 0) {
    return SendMessageW(sci, message, wParam, lParam);
}

enum class Tone {
    Default,
    Comment,
    Keyword,
    Type,
    String,
    Number,
    Operator,
    Preprocessor,
    Tag,
    Attribute,
    Function,
    Variable,
    Constant,
    Error,
    Header,
    Link,
    Added,
    Deleted,
};

COLORREF ColorFor(Tone tone) {
    switch (tone) {
    case Tone::Comment: return theme::kSyntaxComment;
    case Tone::Keyword: return theme::kSyntaxKeyword;
    case Tone::Type: return theme::kSyntaxType;
    case Tone::String: return theme::kSyntaxString;
    case Tone::Number: return theme::kSyntaxNumber;
    case Tone::Operator: return theme::kSyntaxOperator;
    case Tone::Preprocessor: return theme::kSyntaxPreprocessor;
    case Tone::Tag: return theme::kSyntaxTag;
    case Tone::Attribute: return theme::kSyntaxAttribute;
    case Tone::Function: return theme::kSyntaxFunction;
    case Tone::Variable: return theme::kSyntaxVariable;
    case Tone::Constant: return theme::kSyntaxConstant;
    case Tone::Error: return theme::kSyntaxError;
    case Tone::Header: return theme::kSyntaxHeader;
    case Tone::Link: return theme::kSyntaxLink;
    case Tone::Added: return theme::kSyntaxAdded;
    case Tone::Deleted: return theme::kSyntaxDeleted;
    case Tone::Default: break;
    }
    return theme::kText;
}

struct StyleSpec {
    int style;
    Tone tone;
    bool bold = false;
    bool italic = false;
};

void ApplySpec(HWND sci, const StyleSpec& spec) {
    S(sci, SCI_STYLESETFORE, spec.style, ColorFor(spec.tone));
    S(sci, SCI_STYLESETBOLD, spec.style, spec.bold ? TRUE : FALSE);
    S(sci, SCI_STYLESETITALIC, spec.style, spec.italic ? TRUE : FALSE);
}

template <size_t N>
void ApplySpecs(HWND sci, const StyleSpec (&specs)[N]) {
    for (const StyleSpec& spec : specs) {
        ApplySpec(sci, spec);
    }
}

const Lexilla::LexerModule* ModuleFor(Language language) {
    switch (language) {
    case Language::Cpp:
    case Language::CSharp:
    case Language::Java:
    case Language::JavaScript:
    case Language::TypeScript:
    case Language::Go:
    case Language::Kotlin:
    case Language::Swift:
    case Language::Scala:
    case Language::ObjectiveC:
        return &lmCPP;
    case Language::Python: return &lmPython;
    case Language::Json: return &lmJSON;
    case Language::Html:
    case Language::Xml:
    case Language::Php:
        return &lmHTML;
    case Language::Css: return &lmCss;
    case Language::Markdown: return &lmMarkdown;
    case Language::Bash:
    case Language::Dockerfile:
        return &lmBash;
    case Language::Sql: return &lmSQL;
    case Language::Rust: return &lmRust;
    case Language::Ruby: return &lmRuby;
    case Language::Perl: return &lmPerl;
    case Language::Lua: return &lmLua;
    case Language::Yaml: return &lmYAML;
    case Language::Toml: return &lmTOML;
    case Language::Ini: return &lmConf;
    case Language::Makefile: return &lmMake;
    case Language::Batch: return &lmBatch;
    case Language::PowerShell: return &lmPowerShell;
    case Language::Diff: return &lmDiff;
    case Language::CMake: return &lmCmake;
    case Language::Latex: return &lmLatex;
    case Language::R: return &lmR;
    case Language::Haskell: return &lmHaskell;
    case Language::Dart: return &lmDart;
    case Language::Zig: return &lmZig;
    case Language::Nix: return &lmNix;
    case Language::VisualBasic: return &lmVB;
    case Language::Properties: return &lmProps;
    case Language::Assembly: return &lmAsm;
    case Language::Plain: break;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Keyword lists. Lexilla lexers intentionally ship empty word lists; the
// application supplies the language vocabulary through SCI_SETKEYWORDS.

constexpr const char* kCppKeywords =
    "alignas alignof asm auto bool break case catch char char8_t char16_t char32_t class "
    "concept const consteval constexpr constinit const_cast continue co_await co_return "
    "co_yield decltype default delete do double dynamic_cast else enum explicit export extern "
    "false float for friend goto if inline int long mutable namespace new noexcept nullptr "
    "operator private protected public register reinterpret_cast requires return short signed "
    "sizeof static static_assert static_cast struct switch template this thread_local throw true "
    "try typedef typeid typename union unsigned using virtual void volatile wchar_t while "
    "override final import module";

constexpr const char* kCppTypes =
    "std string wstring vector array deque list map multimap set multiset unordered_map "
    "unordered_set shared_ptr unique_ptr weak_ptr optional variant any tuple pair size_t "
    "ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t";

constexpr const char* kCSharpKeywords =
    "abstract as base bool break byte case catch char checked class const continue decimal "
    "default delegate do double else enum event explicit extern false finally fixed float for "
    "foreach goto if implicit in int interface internal is lock long namespace new null object "
    "operator out override params private protected public readonly ref return sbyte sealed "
    "short sizeof stackalloc static string struct switch this throw true try typeof uint ulong "
    "unchecked unsafe ushort using virtual void volatile while async await dynamic get set var "
    "record init required file global";

constexpr const char* kCSharpTypes =
    "List Dictionary HashSet Queue Stack Task ValueTask IEnumerable IReadOnlyList "
    "IReadOnlyDictionary Exception StringBuilder DateTime TimeSpan Guid";

constexpr const char* kJavaKeywords =
    "abstract assert boolean break byte case catch char class const continue default do double "
    "else enum extends final finally float for goto if implements import instanceof int "
    "interface long native new package private protected public return short static strictfp "
    "super switch synchronized this throw throws transient try void volatile while true false "
    "null var record sealed permits yield";

constexpr const char* kJavaTypes =
    "String Object Integer Long Double Float Boolean Character List Map Set ArrayList HashMap "
    "HashSet Optional Stream Exception Runnable Thread";

constexpr const char* kJavaScriptKeywords =
    "break case catch class const continue debugger default delete do else export extends "
    "finally for function if import in instanceof let new return super switch this throw try "
    "typeof var void while with yield async await of static get set from as";

constexpr const char* kTypeScriptKeywords =
    "break case catch class const continue debugger default delete do else export extends "
    "finally for function if import in instanceof let new return super switch this throw try "
    "typeof var void while with yield async await of static get set interface type enum "
    "implements package private protected public abstract as declare keyof readonly namespace "
    "never unknown any string number boolean symbol bigint undefined null true false from";

constexpr const char* kScriptTypes =
    "console document window navigator location localStorage sessionStorage fetch JSON Math "
    "Object Array Promise Map Set WeakMap WeakSet Date RegExp Error String Number Boolean "
    "Symbol BigInt Reflect Proxy Intl";

constexpr const char* kGoKeywords =
    "break case chan const continue default defer else fallthrough for func go goto if import "
    "interface map package range return select struct switch type var true false nil iota";

constexpr const char* kGoTypes =
    "any bool byte comparable complex64 complex128 error float32 float64 int int8 int16 int32 "
    "int64 rune string uint uint8 uint16 uint32 uint64 uintptr make len cap append copy delete "
    "panic recover";

constexpr const char* kKotlinKeywords =
    "package import class interface fun object val var if else when for while do return break "
    "continue throw try catch finally is in as typeof this super null true false abstract final "
    "open override private protected public internal companion data sealed enum annotation init "
    "constructor by where suspend inline noinline crossinline reified lateinit const operator "
    "infix tailrec external actual expect";

constexpr const char* kKotlinTypes =
    "Any Unit Nothing String Int Long Short Byte Double Float Boolean Char List MutableList Map "
    "MutableMap Set MutableSet Array Sequence Flow";

constexpr const char* kSwiftKeywords =
    "associatedtype class deinit enum extension fileprivate func import init inout internal let "
    "open operator private protocol public rethrows static struct subscript typealias var break "
    "case continue default defer do else fallthrough for guard if in repeat return switch where "
    "while as any catch false is nil super self Self throw throws true try async await actor "
    "some nonisolated";

constexpr const char* kSwiftTypes =
    "Bool Int Int8 Int16 Int32 Int64 UInt UInt8 UInt16 UInt32 UInt64 Float Double String "
    "Character Array Dictionary Set Optional Result Error";

constexpr const char* kScalaKeywords =
    "abstract case catch class def do else extends false final finally for forSome if implicit "
    "import lazy match new null object override package private protected return sealed super "
    "this throw trait try true type val var while with yield given using enum export extension "
    "opaque inline transparent derives end";

constexpr const char* kScalaTypes =
    "Any AnyRef AnyVal Boolean Byte Char Double Float Int Long Nothing Short String Unit List "
    "Seq Map Set Option Either Future";

constexpr const char* kObjectiveKeywords =
    "alignas alignof asm auto bool break case catch char char16_t char32_t class const "
    "constexpr const_cast continue decltype default delete do double dynamic_cast else enum "
    "explicit export extern false float for friend goto if inline int long mutable namespace "
    "new noexcept nullptr operator private protected public register reinterpret_cast return "
    "short signed sizeof static static_assert static_cast struct switch template this "
    "thread_local throw true try typedef typeid typename union unsigned using virtual void "
    "volatile wchar_t while id nil Nil BOOL YES NO self super "
    "@interface @implementation @end @property @synthesize @dynamic @protocol @class "
    "@selector @encode @try @catch @finally @throw @synchronized @autoreleasepool";

constexpr const char* kPythonKeywords =
    "False None True and as assert async await break class continue def del elif else except "
    "finally for from global if import in is lambda nonlocal not or pass raise return try "
    "while with yield match case";

constexpr const char* kPythonBuiltins =
    "abs all any ascii bin bool breakpoint bytearray bytes callable chr classmethod compile "
    "complex delattr dict dir divmod enumerate eval exec filter float format frozenset getattr "
    "globals hasattr hash help hex id input int isinstance issubclass iter len list locals map "
    "max memoryview min next object oct open ord pow print property range repr reversed round "
    "set setattr slice sorted staticmethod str sum super tuple type vars zip __init__ self cls";

constexpr const char* kRustKeywords =
    "as break const continue crate else enum extern false fn for if impl in let loop match mod "
    "move mut pub ref return self Self static struct super trait true type unsafe use where "
    "while async await dyn try";

constexpr const char* kRustTypes =
    "bool char f32 f64 i8 i16 i32 i64 i128 isize str u8 u16 u32 u64 u128 usize String Vec "
    "Option Result Box Rc Arc RefCell Cell HashMap HashSet BTreeMap BTreeSet";

constexpr const char* kSqlKeywords =
    "add all alter and any as asc backup between by case check column commit constraint create "
    "database default delete desc distinct drop exec exists foreign from full group having if "
    "in index inner insert into is join key left like limit not null offset on or order outer "
    "primary procedure references right rollback select set table then top transaction trigger "
    "union unique update values view when where with";

constexpr const char* kSqlTypes =
    "bigint binary bit blob boolean char date datetime decimal double float int integer json "
    "numeric nvarchar real smallint text time timestamp tinyint uuid varchar";

constexpr const char* kBashKeywords =
    "if then else elif fi for while until do done case esac function select in time coproc "
    "break continue return exit export local readonly declare typeset unset shift source alias "
    "unalias set echo cd pwd read printf test true false command builtin exec eval trap wait "
    "jobs kill let getopts";

constexpr const char* kDockerKeywords =
    "FROM RUN CMD LABEL MAINTAINER EXPOSE ENV ADD COPY ENTRYPOINT VOLUME USER WORKDIR ARG "
    "ONBUILD STOPSIGNAL HEALTHCHECK SHELL AS";

constexpr const char* kRubyKeywords =
    "BEGIN END alias and begin break case class def defined? do else elsif end ensure false "
    "for if in module next nil not or redo rescue retry return self super then true undef "
    "unless until when while yield require require_relative attr_accessor attr_reader "
    "attr_writer puts print";

constexpr const char* kPerlKeywords =
    "my our local sub use no package require if elsif else unless while until for foreach do "
    "last next redo return print say open close die warn eval exists delete defined undef ref "
    "bless wantarray chomp chop split join map grep sort keys values each push pop shift unshift "
    "splice scalar state given when default";

constexpr const char* kLuaKeywords =
    "and break do else elseif end false for function goto if in local nil not or repeat return "
    "then true until while self require print pairs ipairs type tostring tonumber pcall xpcall "
    "error assert select setmetatable getmetatable rawget rawset rawequal rawlen next";

constexpr const char* kYamlKeywords =
    "true false null yes no on off True False Null Yes No On Off";

constexpr const char* kTomlKeywords =
    "true false inf nan";

constexpr const char* kIniDirectives =
    "include if else endif define undef import";

constexpr const char* kIniParameters =
    "on off true false yes no";

constexpr const char* kMakeDirectives =
    "include define endef ifdef ifndef ifeq ifneq else endif export unexport override vpath "
    "undefine";

constexpr const char* kBatchCommands =
    "echo set if else for goto call exit cd chdir md mkdir rd rmdir del erase copy move ren "
    "rename type dir cls pause start shift setlocal endlocal rem title color path prompt ver vol "
    "break pushd popd assoc ftype";

constexpr const char* kPowerShellKeywords =
    "begin break catch class continue data define do dynamicparam else elseif end enum exit "
    "filter finally for foreach from function hidden if in param process return static switch "
    "throw trap try until using var while workflow";

constexpr const char* kCMakeCommands =
    "add_executable add_library add_subdirectory cmake_minimum_required project set unset if "
    "elseif else endif foreach endforeach while endwhile function endfunction macro endmacro "
    "include find_package target_link_libraries target_include_directories "
    "target_compile_definitions target_compile_options install message option file "
    "configure_file list string return break continue";

constexpr const char* kRKeywords =
    "if else repeat while function for in next break TRUE FALSE NULL Inf NaN NA NA_integer_ "
    "NA_real_ NA_character_ library require return print cat c list data.frame matrix factor "
    "mean median sd var sum min max";

constexpr const char* kHaskellKeywords =
    "case class data default deriving do else if import in infix infixl infixr instance let "
    "module newtype of then type where forall foreign family role pattern static group by using "
    "qualified as hiding";

constexpr const char* kDartKeywords =
    "abstract as assert async await break case catch class const continue covariant default "
    "deferred do dynamic else enum export extends extension external factory false final "
    "finally for Function get hide if implements import in interface is late library mixin new "
    "null on operator part required rethrow return sealed set show static super switch sync "
    "this throw true try typedef var void when while with yield";

constexpr const char* kZigKeywords =
    "addrspace align allowzero and anyframe anytype asm async await break callconv catch "
    "comptime const continue defer else enum errdefer error export extern fn for if inline "
    "linksection noalias noinline nosuspend opaque or orelse packed pub resume return struct "
    "suspend switch test threadlocal try union unreachable usingnamespace var volatile while";

constexpr const char* kNixKeywords =
    "assert else if in inherit let or rec then with true false null";

constexpr const char* kAssemblyKeywords =
    "mov push pop call ret jmp je jne jz jnz jg jge jl jle ja jae jb jbe cmp test add sub mul "
    "imul div idiv and or xor not neg shl shr sal sar lea inc dec nop int syscall leave enter "
    "loop rep repe repne movs stos lods cmps scas setc setnc sete setne setg setge setl setle "
    "seta setae setb setbe xchg xadd cmpxchg bswap adc sbb rol ror rcl rcr bt bts btr btc "
    "clc stc cli sti cld std pushf popf pusha popa";

constexpr const char* kAssemblyRegisters =
    "rax rbx rcx rdx rsi rdi rbp rsp rip eax ebx ecx edx esi edi ebp esp eip ax bx cx dx si "
    "di bp sp al ah bl bh cl ch dl dh r8 r9 r10 r11 r12 r13 r14 r15 r8d r9d r10d r11d r12d "
    "r13d r14d r15d xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 ymm0 ymm1 ymm2 ymm3";

constexpr const char* kVisualBasicKeywords =
    "AddHandler AddressOf Alias And AndAlso As Boolean ByRef Byte ByVal Call Case Catch CBool "
    "CByte CChar CDate CDbl CDec Char CInt Class CLng CObj Const Continue CSByte CShort CSng "
    "CStr CType CUInt CULng CUShort Date Decimal Declare Default Delegate Dim DirectCast Do "
    "Double Each Else ElseIf End EndIf Enum Erase Error Event Exit False Finally For Friend "
    "Function Get GetType Global GoSub GoTo Handles If Implements Imports In Inherits Integer "
    "Interface Is IsNot Let Lib Like Long Loop Me Mod Module MustInherit MustOverride MyBase "
    "MyClass Namespace Narrowing New Next Not Nothing NotInheritable NotOverridable Object Of "
    "On Operator Option Optional Or OrElse Out Overloads Overridable Overrides ParamArray "
    "Partial Private Property Protected Public RaiseEvent ReadOnly ReDim REM RemoveHandler "
    "Resume Return SByte Select Set Shadows Shared Short Single Static Step Stop String "
    "Structure Sub SyncLock Then Throw To True Try TryCast TypeOf UInteger ULong UShort Using "
    "Variant Wend When While Widening With WithEvents WriteOnly Xor";

void SetKeyword(HWND sci, int index, const char* words) {
    if (words && *words) {
        S(sci, SCI_SETKEYWORDS, index, reinterpret_cast<sptr_t>(words));
    }
}

void SetKeywords(HWND sci, Language language) {
    switch (language) {
    case Language::Cpp:
        SetKeyword(sci, 0, kCppKeywords);
        SetKeyword(sci, 1, kCppTypes);
        break;
    case Language::ObjectiveC:
        SetKeyword(sci, 0, kObjectiveKeywords);
        SetKeyword(sci, 1, kCppTypes);
        break;
    case Language::CSharp:
        SetKeyword(sci, 0, kCSharpKeywords);
        SetKeyword(sci, 1, kCSharpTypes);
        break;
    case Language::Java:
        SetKeyword(sci, 0, kJavaKeywords);
        SetKeyword(sci, 1, kJavaTypes);
        break;
    case Language::JavaScript:
        SetKeyword(sci, 0, kJavaScriptKeywords);
        SetKeyword(sci, 1, kScriptTypes);
        break;
    case Language::TypeScript:
        SetKeyword(sci, 0, kTypeScriptKeywords);
        SetKeyword(sci, 1, kScriptTypes);
        break;
    case Language::Go:
        SetKeyword(sci, 0, kGoKeywords);
        SetKeyword(sci, 1, kGoTypes);
        break;
    case Language::Kotlin:
        SetKeyword(sci, 0, kKotlinKeywords);
        SetKeyword(sci, 1, kKotlinTypes);
        break;
    case Language::Swift:
        SetKeyword(sci, 0, kSwiftKeywords);
        SetKeyword(sci, 1, kSwiftTypes);
        break;
    case Language::Scala:
        SetKeyword(sci, 0, kScalaKeywords);
        SetKeyword(sci, 1, kScalaTypes);
        break;
    case Language::Python:
        SetKeyword(sci, 0, kPythonKeywords);
        SetKeyword(sci, 1, kPythonBuiltins);
        break;
    case Language::Rust:
        SetKeyword(sci, 0, kRustKeywords);
        SetKeyword(sci, 1, kRustTypes);
        break;
    case Language::Sql:
        SetKeyword(sci, 0, kSqlKeywords);
        SetKeyword(sci, 1, kSqlTypes);
        break;
    case Language::Bash:
        SetKeyword(sci, 0, kBashKeywords);
        break;
    case Language::Dockerfile:
        SetKeyword(sci, 0, kDockerKeywords);
        break;
    case Language::Ruby:
        SetKeyword(sci, 0, kRubyKeywords);
        break;
    case Language::Perl:
        SetKeyword(sci, 0, kPerlKeywords);
        break;
    case Language::Lua:
        SetKeyword(sci, 0, kLuaKeywords);
        break;
    case Language::Yaml:
        SetKeyword(sci, 0, kYamlKeywords);
        break;
    case Language::Toml:
        SetKeyword(sci, 0, kTomlKeywords);
        break;
    case Language::Ini:
        SetKeyword(sci, 0, kIniDirectives);
        SetKeyword(sci, 1, kIniParameters);
        break;
    case Language::Makefile:
        SetKeyword(sci, 0, kMakeDirectives);
        break;
    case Language::Batch:
        SetKeyword(sci, 0, kBatchCommands);
        break;
    case Language::PowerShell:
        SetKeyword(sci, 0, kPowerShellKeywords);
        break;
    case Language::CMake:
        SetKeyword(sci, 0, kCMakeCommands);
        break;
    case Language::R:
        SetKeyword(sci, 0, kRKeywords);
        break;
    case Language::Haskell:
        SetKeyword(sci, 0, kHaskellKeywords);
        break;
    case Language::Dart:
        SetKeyword(sci, 0, kDartKeywords);
        break;
    case Language::Zig:
        SetKeyword(sci, 0, kZigKeywords);
        break;
    case Language::Nix:
        SetKeyword(sci, 0, kNixKeywords);
        break;
    case Language::Assembly:
        SetKeyword(sci, 0, kAssemblyKeywords);
        SetKeyword(sci, 2, kAssemblyRegisters);
        break;
    case Language::VisualBasic:
        SetKeyword(sci, 0, kVisualBasicKeywords);
        break;
    case Language::Html:
    case Language::Xml:
    case Language::Php:
        SetKeyword(sci, 1, kJavaScriptKeywords);
        SetKeyword(sci, 4, "php echo print foreach as function class extends implements "
                            "public private protected static const var new return if else "
                            "elseif while for switch case break continue require include "
                            "namespace use try catch finally throw true false null");
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Style maps. Scintilla style numbers are lexer-specific, so the important
// styles for each bundled lexer are listed explicitly.

constexpr StyleSpec kCppStyles[] = {
    {SCE_C_COMMENT, Tone::Comment},
    {SCE_C_COMMENTLINE, Tone::Comment},
    {SCE_C_COMMENTDOC, Tone::Comment},
    {SCE_C_COMMENTLINEDOC, Tone::Comment},
    {SCE_C_COMMENTDOCKEYWORD, Tone::Comment},
    {SCE_C_COMMENTDOCKEYWORDERROR, Tone::Error},
    {SCE_C_PREPROCESSORCOMMENT, Tone::Comment},
    {SCE_C_PREPROCESSORCOMMENTDOC, Tone::Comment},
    {SCE_C_NUMBER, Tone::Number},
    {SCE_C_WORD, Tone::Keyword},
    {SCE_C_WORD2, Tone::Type},
    {SCE_C_STRING, Tone::String},
    {SCE_C_CHARACTER, Tone::String},
    {SCE_C_STRINGEOL, Tone::String},
    {SCE_C_VERBATIM, Tone::String},
    {SCE_C_STRINGRAW, Tone::String},
    {SCE_C_TRIPLEVERBATIM, Tone::String},
    {SCE_C_HASHQUOTEDSTRING, Tone::String},
    {SCE_C_REGEX, Tone::String},
    {SCE_C_UUID, Tone::Constant},
    {SCE_C_PREPROCESSOR, Tone::Preprocessor},
    {SCE_C_OPERATOR, Tone::Operator},
    {SCE_C_IDENTIFIER, Tone::Default},
    {SCE_C_GLOBALCLASS, Tone::Type},
    {SCE_C_USERLITERAL, Tone::Constant},
    {SCE_C_TASKMARKER, Tone::Comment, true},
    {SCE_C_ESCAPESEQUENCE, Tone::Constant},
};

constexpr StyleSpec kPythonStyles[] = {
    {SCE_P_COMMENTLINE, Tone::Comment},
    {SCE_P_COMMENTBLOCK, Tone::Comment},
    {SCE_P_NUMBER, Tone::Number},
    {SCE_P_STRING, Tone::String},
    {SCE_P_CHARACTER, Tone::String},
    {SCE_P_WORD, Tone::Keyword},
    {SCE_P_TRIPLE, Tone::String},
    {SCE_P_TRIPLEDOUBLE, Tone::String},
    {SCE_P_CLASSNAME, Tone::Type},
    {SCE_P_DEFNAME, Tone::Function},
    {SCE_P_OPERATOR, Tone::Operator},
    {SCE_P_IDENTIFIER, Tone::Default},
    {SCE_P_STRINGEOL, Tone::String},
    {SCE_P_WORD2, Tone::Type},
    {SCE_P_DECORATOR, Tone::Preprocessor},
    {SCE_P_FSTRING, Tone::String},
    {SCE_P_FCHARACTER, Tone::String},
    {SCE_P_FTRIPLE, Tone::String},
    {SCE_P_FTRIPLEDOUBLE, Tone::String},
    {SCE_P_ATTRIBUTE, Tone::Variable},
};

constexpr StyleSpec kHtmlStyles[] = {
    {SCE_H_TAG, Tone::Tag},
    {SCE_H_TAGUNKNOWN, Tone::Error},
    {SCE_H_ATTRIBUTE, Tone::Attribute},
    {SCE_H_ATTRIBUTEUNKNOWN, Tone::Error},
    {SCE_H_NUMBER, Tone::Number},
    {SCE_H_DOUBLESTRING, Tone::String},
    {SCE_H_SINGLESTRING, Tone::String},
    {SCE_H_COMMENT, Tone::Comment},
    {SCE_H_XCCOMMENT, Tone::Comment},
    {SCE_H_ENTITY, Tone::Constant},
    {SCE_H_TAGEND, Tone::Tag},
    {SCE_H_XMLSTART, Tone::Tag},
    {SCE_H_XMLEND, Tone::Tag},
    {SCE_H_SCRIPT, Tone::Preprocessor},
    {SCE_H_ASP, Tone::Preprocessor},
    {SCE_H_ASPAT, Tone::Preprocessor},
    {SCE_H_CDATA, Tone::String},
    {SCE_H_QUESTION, Tone::Operator},
    {SCE_H_VALUE, Tone::String},
    {SCE_HJ_COMMENT, Tone::Comment},
    {SCE_HJ_COMMENTLINE, Tone::Comment},
    {SCE_HJ_COMMENTDOC, Tone::Comment},
    {SCE_HJ_NUMBER, Tone::Number},
    {SCE_HJ_WORD, Tone::Default},
    {SCE_HJ_KEYWORD, Tone::Keyword},
    {SCE_HJ_DOUBLESTRING, Tone::String},
    {SCE_HJ_SINGLESTRING, Tone::String},
    {SCE_HJ_SYMBOLS, Tone::Operator},
    {SCE_HJ_STRINGEOL, Tone::String},
    {SCE_HJ_REGEX, Tone::String},
    {SCE_HJ_TEMPLATELITERAL, Tone::String},
    {SCE_HJA_COMMENT, Tone::Comment},
    {SCE_HJA_COMMENTLINE, Tone::Comment},
    {SCE_HJA_COMMENTDOC, Tone::Comment},
    {SCE_HJA_NUMBER, Tone::Number},
    {SCE_HJA_KEYWORD, Tone::Keyword},
    {SCE_HJA_DOUBLESTRING, Tone::String},
    {SCE_HJA_SINGLESTRING, Tone::String},
    {SCE_HJA_SYMBOLS, Tone::Operator},
    {SCE_HJA_STRINGEOL, Tone::String},
    {SCE_HJA_REGEX, Tone::String},
    {SCE_HJA_TEMPLATELITERAL, Tone::String},
    {SCE_HB_COMMENTLINE, Tone::Comment},
    {SCE_HB_NUMBER, Tone::Number},
    {SCE_HB_WORD, Tone::Keyword},
    {SCE_HB_STRING, Tone::String},
    {SCE_HB_STRINGEOL, Tone::String},
    {SCE_HBA_COMMENTLINE, Tone::Comment},
    {SCE_HBA_NUMBER, Tone::Number},
    {SCE_HBA_WORD, Tone::Keyword},
    {SCE_HBA_STRING, Tone::String},
    {SCE_HBA_STRINGEOL, Tone::String},
    {SCE_HP_COMMENTLINE, Tone::Comment},
    {SCE_HP_NUMBER, Tone::Number},
    {SCE_HP_STRING, Tone::String},
    {SCE_HP_CHARACTER, Tone::String},
    {SCE_HP_WORD, Tone::Keyword},
    {SCE_HP_TRIPLE, Tone::String},
    {SCE_HP_TRIPLEDOUBLE, Tone::String},
    {SCE_HP_CLASSNAME, Tone::Type},
    {SCE_HP_DEFNAME, Tone::Function},
    {SCE_HP_OPERATOR, Tone::Operator},
    {SCE_HPA_COMMENTLINE, Tone::Comment},
    {SCE_HPA_NUMBER, Tone::Number},
    {SCE_HPA_STRING, Tone::String},
    {SCE_HPA_CHARACTER, Tone::String},
    {SCE_HPA_WORD, Tone::Keyword},
    {SCE_HPA_TRIPLE, Tone::String},
    {SCE_HPA_TRIPLEDOUBLE, Tone::String},
    {SCE_HPA_CLASSNAME, Tone::Type},
    {SCE_HPA_DEFNAME, Tone::Function},
    {SCE_HPA_OPERATOR, Tone::Operator},
};

constexpr StyleSpec kCssStyles[] = {
    {SCE_CSS_TAG, Tone::Tag},
    {SCE_CSS_CLASS, Tone::Function},
    {SCE_CSS_PSEUDOCLASS, Tone::Keyword},
    {SCE_CSS_UNKNOWN_PSEUDOCLASS, Tone::Error},
    {SCE_CSS_OPERATOR, Tone::Operator},
    {SCE_CSS_IDENTIFIER, Tone::Variable},
    {SCE_CSS_UNKNOWN_IDENTIFIER, Tone::Variable},
    {SCE_CSS_VALUE, Tone::Constant},
    {SCE_CSS_COMMENT, Tone::Comment},
    {SCE_CSS_ID, Tone::Tag},
    {SCE_CSS_IMPORTANT, Tone::Keyword, true},
    {SCE_CSS_DIRECTIVE, Tone::Preprocessor},
    {SCE_CSS_DOUBLESTRING, Tone::String},
    {SCE_CSS_SINGLESTRING, Tone::String},
    {SCE_CSS_IDENTIFIER2, Tone::Variable},
    {SCE_CSS_ATTRIBUTE, Tone::Attribute},
    {SCE_CSS_IDENTIFIER3, Tone::Variable},
    {SCE_CSS_PSEUDOELEMENT, Tone::Keyword},
    {SCE_CSS_EXTENDED_IDENTIFIER, Tone::Variable},
    {SCE_CSS_EXTENDED_PSEUDOCLASS, Tone::Keyword},
    {SCE_CSS_EXTENDED_PSEUDOELEMENT, Tone::Keyword},
    {SCE_CSS_GROUP_RULE, Tone::Default},
    {SCE_CSS_VARIABLE, Tone::Variable},
};

constexpr StyleSpec kJsonStyles[] = {
    {SCE_JSON_NUMBER, Tone::Number},
    {SCE_JSON_STRING, Tone::String},
    {SCE_JSON_STRINGEOL, Tone::String},
    {SCE_JSON_PROPERTYNAME, Tone::Variable},
    {SCE_JSON_ESCAPESEQUENCE, Tone::Constant},
    {SCE_JSON_LINECOMMENT, Tone::Comment},
    {SCE_JSON_BLOCKCOMMENT, Tone::Comment},
    {SCE_JSON_OPERATOR, Tone::Operator},
    {SCE_JSON_URI, Tone::Link},
    {SCE_JSON_COMPACTIRI, Tone::Link},
    {SCE_JSON_KEYWORD, Tone::Keyword},
    {SCE_JSON_LDKEYWORD, Tone::Keyword},
    {SCE_JSON_ERROR, Tone::Error},
};

constexpr StyleSpec kMarkdownStyles[] = {
    {SCE_MARKDOWN_STRONG1, Tone::Default, true},
    {SCE_MARKDOWN_STRONG2, Tone::Default, true},
    {SCE_MARKDOWN_EM1, Tone::Default, false, true},
    {SCE_MARKDOWN_EM2, Tone::Default, false, true},
    {SCE_MARKDOWN_HEADER1, Tone::Header, true},
    {SCE_MARKDOWN_HEADER2, Tone::Header, true},
    {SCE_MARKDOWN_HEADER3, Tone::Header, true},
    {SCE_MARKDOWN_HEADER4, Tone::Header, true},
    {SCE_MARKDOWN_HEADER5, Tone::Header, true},
    {SCE_MARKDOWN_HEADER6, Tone::Header, true},
    {SCE_MARKDOWN_PRECHAR, Tone::Operator},
    {SCE_MARKDOWN_ULIST_ITEM, Tone::Keyword},
    {SCE_MARKDOWN_OLIST_ITEM, Tone::Keyword},
    {SCE_MARKDOWN_BLOCKQUOTE, Tone::Comment, false, true},
    {SCE_MARKDOWN_STRIKEOUT, Tone::Comment},
    {SCE_MARKDOWN_HRULE, Tone::Operator},
    {SCE_MARKDOWN_LINK, Tone::Link},
    {SCE_MARKDOWN_CODE, Tone::String},
    {SCE_MARKDOWN_CODE2, Tone::String},
    {SCE_MARKDOWN_CODEBK, Tone::String},
    {SCE_MARKDOWN_FRONT_MARK, Tone::Comment},
    {SCE_MARKDOWN_FRONT, Tone::Comment},
    {SCE_MARKDOWN_FRONT_KEY, Tone::Variable},
};

constexpr StyleSpec kBashStyles[] = {
    {SCE_SH_ERROR, Tone::Error},
    {SCE_SH_COMMENTLINE, Tone::Comment},
    {SCE_SH_NUMBER, Tone::Number},
    {SCE_SH_WORD, Tone::Keyword},
    {SCE_SH_STRING, Tone::String},
    {SCE_SH_CHARACTER, Tone::String},
    {SCE_SH_OPERATOR, Tone::Operator},
    {SCE_SH_IDENTIFIER, Tone::Default},
    {SCE_SH_SCALAR, Tone::Variable},
    {SCE_SH_PARAM, Tone::Variable},
    {SCE_SH_BACKTICKS, Tone::String},
    {SCE_SH_HERE_DELIM, Tone::Comment},
    {SCE_SH_HERE_Q, Tone::String},
};

constexpr StyleSpec kSqlStyles[] = {
    {SCE_SQL_COMMENT, Tone::Comment},
    {SCE_SQL_COMMENTLINE, Tone::Comment},
    {SCE_SQL_COMMENTDOC, Tone::Comment},
    {SCE_SQL_COMMENTLINEDOC, Tone::Comment},
    {SCE_SQL_COMMENTDOCKEYWORD, Tone::Comment},
    {SCE_SQL_COMMENTDOCKEYWORDERROR, Tone::Error},
    {SCE_SQL_NUMBER, Tone::Number},
    {SCE_SQL_WORD, Tone::Keyword},
    {SCE_SQL_WORD2, Tone::Type},
    {SCE_SQL_STRING, Tone::String},
    {SCE_SQL_CHARACTER, Tone::String},
    {SCE_SQL_SQLPLUS, Tone::Preprocessor},
    {SCE_SQL_SQLPLUS_PROMPT, Tone::Comment},
    {SCE_SQL_OPERATOR, Tone::Operator},
    {SCE_SQL_IDENTIFIER, Tone::Default},
    {SCE_SQL_SQLPLUS_COMMENT, Tone::Comment},
    {SCE_SQL_USER1, Tone::Variable},
    {SCE_SQL_USER2, Tone::Variable},
    {SCE_SQL_USER3, Tone::Variable},
    {SCE_SQL_USER4, Tone::Variable},
    {SCE_SQL_QUOTEDIDENTIFIER, Tone::Variable},
    {SCE_SQL_QOPERATOR, Tone::Operator},
};

constexpr StyleSpec kRustStyles[] = {
    {SCE_RUST_COMMENTBLOCK, Tone::Comment},
    {SCE_RUST_COMMENTLINE, Tone::Comment},
    {SCE_RUST_COMMENTBLOCKDOC, Tone::Comment},
    {SCE_RUST_COMMENTLINEDOC, Tone::Comment},
    {SCE_RUST_NUMBER, Tone::Number},
    {SCE_RUST_WORD, Tone::Keyword},
    {SCE_RUST_WORD2, Tone::Type},
    {SCE_RUST_WORD3, Tone::Type},
    {SCE_RUST_WORD4, Tone::Type},
    {SCE_RUST_WORD5, Tone::Type},
    {SCE_RUST_WORD6, Tone::Type},
    {SCE_RUST_WORD7, Tone::Type},
    {SCE_RUST_STRING, Tone::String},
    {SCE_RUST_STRINGR, Tone::String},
    {SCE_RUST_CHARACTER, Tone::String},
    {SCE_RUST_OPERATOR, Tone::Operator},
    {SCE_RUST_IDENTIFIER, Tone::Default},
    {SCE_RUST_LIFETIME, Tone::Variable},
    {SCE_RUST_MACRO, Tone::Function},
    {SCE_RUST_LEXERROR, Tone::Error},
    {SCE_RUST_BYTESTRING, Tone::String},
    {SCE_RUST_BYTESTRINGR, Tone::String},
    {SCE_RUST_BYTECHARACTER, Tone::String},
    {SCE_RUST_CSTRING, Tone::String},
    {SCE_RUST_CSTRINGR, Tone::String},
};

constexpr StyleSpec kRubyStyles[] = {
    {SCE_RB_ERROR, Tone::Error},
    {SCE_RB_COMMENTLINE, Tone::Comment},
    {SCE_RB_POD, Tone::Comment},
    {SCE_RB_NUMBER, Tone::Number},
    {SCE_RB_WORD, Tone::Keyword},
    {SCE_RB_STRING, Tone::String},
    {SCE_RB_CHARACTER, Tone::String},
    {SCE_RB_CLASSNAME, Tone::Type},
    {SCE_RB_DEFNAME, Tone::Function},
    {SCE_RB_OPERATOR, Tone::Operator},
    {SCE_RB_IDENTIFIER, Tone::Default},
    {SCE_RB_REGEX, Tone::String},
    {SCE_RB_GLOBAL, Tone::Variable},
    {SCE_RB_SYMBOL, Tone::Constant},
    {SCE_RB_MODULE_NAME, Tone::Type},
    {SCE_RB_INSTANCE_VAR, Tone::Variable},
    {SCE_RB_CLASS_VAR, Tone::Variable},
    {SCE_RB_BACKTICKS, Tone::String},
    {SCE_RB_DATASECTION, Tone::Comment},
    {SCE_RB_HERE_DELIM, Tone::Comment},
    {SCE_RB_HERE_Q, Tone::String},
    {SCE_RB_HERE_QQ, Tone::String},
    {SCE_RB_HERE_QX, Tone::String},
    {SCE_RB_STRING_Q, Tone::String},
    {SCE_RB_STRING_QQ, Tone::String},
    {SCE_RB_STRING_QX, Tone::String},
    {SCE_RB_STRING_QR, Tone::String},
    {SCE_RB_STRING_QW, Tone::String},
    {SCE_RB_WORD_DEMOTED, Tone::Keyword},
    {SCE_RB_STDIN, Tone::Constant},
    {SCE_RB_STDOUT, Tone::Constant},
    {SCE_RB_STDERR, Tone::Constant},
    {SCE_RB_STRING_W, Tone::String},
    {SCE_RB_STRING_I, Tone::String},
    {SCE_RB_STRING_QI, Tone::String},
    {SCE_RB_STRING_QS, Tone::String},
};

constexpr StyleSpec kPerlStyles[] = {
    {SCE_PL_ERROR, Tone::Error},
    {SCE_PL_COMMENTLINE, Tone::Comment},
    {SCE_PL_POD, Tone::Comment},
    {SCE_PL_POD_VERB, Tone::Comment},
    {SCE_PL_NUMBER, Tone::Number},
    {SCE_PL_WORD, Tone::Keyword},
    {SCE_PL_STRING, Tone::String},
    {SCE_PL_CHARACTER, Tone::String},
    {SCE_PL_PUNCTUATION, Tone::Operator},
    {SCE_PL_PREPROCESSOR, Tone::Preprocessor},
    {SCE_PL_OPERATOR, Tone::Operator},
    {SCE_PL_IDENTIFIER, Tone::Default},
    {SCE_PL_SCALAR, Tone::Variable},
    {SCE_PL_ARRAY, Tone::Variable},
    {SCE_PL_HASH, Tone::Variable},
    {SCE_PL_SYMBOLTABLE, Tone::Variable},
    {SCE_PL_VARIABLE_INDEXER, Tone::Variable},
    {SCE_PL_REGEX, Tone::String},
    {SCE_PL_REGSUBST, Tone::String},
    {SCE_PL_LONGQUOTE, Tone::String},
    {SCE_PL_BACKTICKS, Tone::String},
    {SCE_PL_DATASECTION, Tone::Comment},
    {SCE_PL_HERE_DELIM, Tone::Comment},
    {SCE_PL_HERE_Q, Tone::String},
    {SCE_PL_HERE_QQ, Tone::String},
    {SCE_PL_HERE_QX, Tone::String},
    {SCE_PL_STRING_Q, Tone::String},
    {SCE_PL_STRING_QQ, Tone::String},
    {SCE_PL_STRING_QX, Tone::String},
    {SCE_PL_STRING_QR, Tone::String},
    {SCE_PL_STRING_QW, Tone::String},
    {SCE_PL_SUB_PROTOTYPE, Tone::Type},
    {SCE_PL_FORMAT_IDENT, Tone::Function},
    {SCE_PL_FORMAT, Tone::String},
    {SCE_PL_STRING_VAR, Tone::Variable},
    {SCE_PL_XLAT, Tone::String},
    {SCE_PL_REGEX_VAR, Tone::Variable},
    {SCE_PL_REGSUBST_VAR, Tone::Variable},
    {SCE_PL_BACKTICKS_VAR, Tone::Variable},
    {SCE_PL_HERE_QQ_VAR, Tone::Variable},
    {SCE_PL_HERE_QX_VAR, Tone::Variable},
    {SCE_PL_STRING_QQ_VAR, Tone::Variable},
    {SCE_PL_STRING_QX_VAR, Tone::Variable},
    {SCE_PL_STRING_QR_VAR, Tone::Variable},
};

constexpr StyleSpec kLuaStyles[] = {
    {SCE_LUA_COMMENT, Tone::Comment},
    {SCE_LUA_COMMENTLINE, Tone::Comment},
    {SCE_LUA_COMMENTDOC, Tone::Comment},
    {SCE_LUA_NUMBER, Tone::Number},
    {SCE_LUA_WORD, Tone::Keyword},
    {SCE_LUA_STRING, Tone::String},
    {SCE_LUA_CHARACTER, Tone::String},
    {SCE_LUA_LITERALSTRING, Tone::String},
    {SCE_LUA_PREPROCESSOR, Tone::Preprocessor},
    {SCE_LUA_OPERATOR, Tone::Operator},
    {SCE_LUA_IDENTIFIER, Tone::Default},
    {SCE_LUA_STRINGEOL, Tone::String},
    {SCE_LUA_WORD2, Tone::Function},
    {SCE_LUA_WORD3, Tone::Function},
    {SCE_LUA_WORD4, Tone::Function},
    {SCE_LUA_WORD5, Tone::Function},
    {SCE_LUA_WORD6, Tone::Function},
    {SCE_LUA_WORD7, Tone::Function},
    {SCE_LUA_WORD8, Tone::Function},
    {SCE_LUA_LABEL, Tone::Variable},
};

constexpr StyleSpec kYamlStyles[] = {
    {SCE_YAML_COMMENT, Tone::Comment},
    {SCE_YAML_IDENTIFIER, Tone::Variable},
    {SCE_YAML_KEYWORD, Tone::Keyword},
    {SCE_YAML_NUMBER, Tone::Number},
    {SCE_YAML_REFERENCE, Tone::Link},
    {SCE_YAML_DOCUMENT, Tone::Preprocessor},
    {SCE_YAML_TEXT, Tone::String},
    {SCE_YAML_ERROR, Tone::Error},
    {SCE_YAML_OPERATOR, Tone::Operator},
};

constexpr StyleSpec kTomlStyles[] = {
    {SCE_TOML_COMMENT, Tone::Comment},
    {SCE_TOML_IDENTIFIER, Tone::Variable},
    {SCE_TOML_KEYWORD, Tone::Keyword},
    {SCE_TOML_NUMBER, Tone::Number},
    {SCE_TOML_TABLE, Tone::Header},
    {SCE_TOML_KEY, Tone::Attribute},
    {SCE_TOML_ERROR, Tone::Error},
    {SCE_TOML_OPERATOR, Tone::Operator},
    {SCE_TOML_STRING_SQ, Tone::String},
    {SCE_TOML_STRING_DQ, Tone::String},
    {SCE_TOML_TRIPLE_STRING_SQ, Tone::String},
    {SCE_TOML_TRIPLE_STRING_DQ, Tone::String},
    {SCE_TOML_ESCAPECHAR, Tone::Constant},
    {SCE_TOML_DATETIME, Tone::Constant},
    {SCE_TOML_STRINGEOL, Tone::String},
};

constexpr StyleSpec kIniStyles[] = {
    {SCE_CONF_COMMENT, Tone::Comment},
    {SCE_CONF_NUMBER, Tone::Number},
    {SCE_CONF_IDENTIFIER, Tone::Variable},
    {SCE_CONF_EXTENSION, Tone::Keyword},
    {SCE_CONF_PARAMETER, Tone::Type},
    {SCE_CONF_STRING, Tone::String},
    {SCE_CONF_OPERATOR, Tone::Operator},
    {SCE_CONF_IP, Tone::Constant},
    {SCE_CONF_DIRECTIVE, Tone::Preprocessor},
};

constexpr StyleSpec kMakeStyles[] = {
    {SCE_MAKE_COMMENT, Tone::Comment},
    {SCE_MAKE_PREPROCESSOR, Tone::Preprocessor},
    {SCE_MAKE_IDENTIFIER, Tone::Variable},
    {SCE_MAKE_OPERATOR, Tone::Operator},
    {SCE_MAKE_TARGET, Tone::Function},
    {SCE_MAKE_IDEOL, Tone::Comment},
};

constexpr StyleSpec kBatchStyles[] = {
    {SCE_BAT_COMMENT, Tone::Comment},
    {SCE_BAT_WORD, Tone::Keyword},
    {SCE_BAT_LABEL, Tone::Function},
    {SCE_BAT_HIDE, Tone::Comment},
    {SCE_BAT_COMMAND, Tone::Function},
    {SCE_BAT_IDENTIFIER, Tone::Variable},
    {SCE_BAT_OPERATOR, Tone::Operator},
    {SCE_BAT_AFTER_LABEL, Tone::Default},
};

constexpr StyleSpec kPowerShellStyles[] = {
    {SCE_POWERSHELL_COMMENT, Tone::Comment},
    {SCE_POWERSHELL_COMMENTSTREAM, Tone::Comment},
    {SCE_POWERSHELL_COMMENTDOCKEYWORD, Tone::Comment},
    {SCE_POWERSHELL_STRING, Tone::String},
    {SCE_POWERSHELL_CHARACTER, Tone::String},
    {SCE_POWERSHELL_HERE_STRING, Tone::String},
    {SCE_POWERSHELL_HERE_CHARACTER, Tone::String},
    {SCE_POWERSHELL_NUMBER, Tone::Number},
    {SCE_POWERSHELL_VARIABLE, Tone::Variable},
    {SCE_POWERSHELL_OPERATOR, Tone::Operator},
    {SCE_POWERSHELL_IDENTIFIER, Tone::Default},
    {SCE_POWERSHELL_KEYWORD, Tone::Keyword},
    {SCE_POWERSHELL_CMDLET, Tone::Function},
    {SCE_POWERSHELL_ALIAS, Tone::Function},
    {SCE_POWERSHELL_FUNCTION, Tone::Function},
    {SCE_POWERSHELL_USER1, Tone::Constant},
};

constexpr StyleSpec kDiffStyles[] = {
    {SCE_DIFF_COMMENT, Tone::Comment},
    {SCE_DIFF_COMMAND, Tone::Preprocessor},
    {SCE_DIFF_HEADER, Tone::Header},
    {SCE_DIFF_POSITION, Tone::Number},
    {SCE_DIFF_DELETED, Tone::Deleted},
    {SCE_DIFF_ADDED, Tone::Added},
    {SCE_DIFF_CHANGED, Tone::Keyword},
    {SCE_DIFF_PATCH_ADD, Tone::Added},
    {SCE_DIFF_PATCH_DELETE, Tone::Deleted},
    {SCE_DIFF_REMOVED_PATCH_ADD, Tone::Added},
    {SCE_DIFF_REMOVED_PATCH_DELETE, Tone::Deleted},
};

constexpr StyleSpec kCMakeStyles[] = {
    {SCE_CMAKE_COMMENT, Tone::Comment},
    {SCE_CMAKE_STRINGDQ, Tone::String},
    {SCE_CMAKE_STRINGLQ, Tone::String},
    {SCE_CMAKE_STRINGRQ, Tone::String},
    {SCE_CMAKE_COMMANDS, Tone::Keyword},
    {SCE_CMAKE_PARAMETERS, Tone::Variable},
    {SCE_CMAKE_VARIABLE, Tone::Variable},
    {SCE_CMAKE_USERDEFINED, Tone::Function},
    {SCE_CMAKE_WHILEDEF, Tone::Keyword},
    {SCE_CMAKE_FOREACHDEF, Tone::Keyword},
    {SCE_CMAKE_IFDEFINEDEF, Tone::Keyword},
    {SCE_CMAKE_MACRODEF, Tone::Keyword},
    {SCE_CMAKE_STRINGVAR, Tone::String},
    {SCE_CMAKE_NUMBER, Tone::Number},
};

constexpr StyleSpec kLatexStyles[] = {
    {SCE_L_COMMAND, Tone::Keyword},
    {SCE_L_TAG, Tone::Tag},
    {SCE_L_MATH, Tone::Number},
    {SCE_L_COMMENT, Tone::Comment},
    {SCE_L_TAG2, Tone::Tag},
    {SCE_L_MATH2, Tone::Number},
    {SCE_L_COMMENT2, Tone::Comment},
    {SCE_L_VERBATIM, Tone::String},
    {SCE_L_SHORTCMD, Tone::Keyword},
    {SCE_L_SPECIAL, Tone::Operator},
    {SCE_L_CMDOPT, Tone::Attribute},
    {SCE_L_ERROR, Tone::Error},
};

constexpr StyleSpec kRStyles[] = {
    {SCE_R_COMMENT, Tone::Comment},
    {SCE_R_KWORD, Tone::Keyword},
    {SCE_R_BASEKWORD, Tone::Type},
    {SCE_R_OTHERKWORD, Tone::Keyword},
    {SCE_R_NUMBER, Tone::Number},
    {SCE_R_STRING, Tone::String},
    {SCE_R_STRING2, Tone::String},
    {SCE_R_OPERATOR, Tone::Operator},
    {SCE_R_IDENTIFIER, Tone::Default},
    {SCE_R_INFIX, Tone::Operator},
    {SCE_R_INFIXEOL, Tone::Operator},
    {SCE_R_BACKTICKS, Tone::String},
    {SCE_R_RAWSTRING, Tone::String},
    {SCE_R_RAWSTRING2, Tone::String},
    {SCE_R_ESCAPESEQUENCE, Tone::Constant},
};

constexpr StyleSpec kHaskellStyles[] = {
    {SCE_HA_IDENTIFIER, Tone::Default},
    {SCE_HA_KEYWORD, Tone::Keyword},
    {SCE_HA_NUMBER, Tone::Number},
    {SCE_HA_STRING, Tone::String},
    {SCE_HA_CHARACTER, Tone::String},
    {SCE_HA_CLASS, Tone::Type},
    {SCE_HA_MODULE, Tone::Type},
    {SCE_HA_CAPITAL, Tone::Type},
    {SCE_HA_DATA, Tone::Type},
    {SCE_HA_IMPORT, Tone::Preprocessor},
    {SCE_HA_OPERATOR, Tone::Operator},
    {SCE_HA_INSTANCE, Tone::Variable},
    {SCE_HA_COMMENTLINE, Tone::Comment},
    {SCE_HA_COMMENTBLOCK, Tone::Comment},
    {SCE_HA_COMMENTBLOCK2, Tone::Comment},
    {SCE_HA_COMMENTBLOCK3, Tone::Comment},
    {SCE_HA_PRAGMA, Tone::Preprocessor},
    {SCE_HA_PREPROCESSOR, Tone::Preprocessor},
    {SCE_HA_STRINGEOL, Tone::String},
    {SCE_HA_RESERVED_OPERATOR, Tone::Operator},
    {SCE_HA_LITERATE_COMMENT, Tone::Comment},
    {SCE_HA_LITERATE_CODEDELIM, Tone::Comment},
};

constexpr StyleSpec kDartStyles[] = {
    {SCE_DART_COMMENTLINE, Tone::Comment},
    {SCE_DART_COMMENTLINEDOC, Tone::Comment},
    {SCE_DART_COMMENTBLOCK, Tone::Comment},
    {SCE_DART_COMMENTBLOCKDOC, Tone::Comment},
    {SCE_DART_STRING_SQ, Tone::String},
    {SCE_DART_STRING_DQ, Tone::String},
    {SCE_DART_TRIPLE_STRING_SQ, Tone::String},
    {SCE_DART_TRIPLE_STRING_DQ, Tone::String},
    {SCE_DART_RAWSTRING_SQ, Tone::String},
    {SCE_DART_RAWSTRING_DQ, Tone::String},
    {SCE_DART_TRIPLE_RAWSTRING_SQ, Tone::String},
    {SCE_DART_TRIPLE_RAWSTRING_DQ, Tone::String},
    {SCE_DART_ESCAPECHAR, Tone::Constant},
    {SCE_DART_IDENTIFIER, Tone::Default},
    {SCE_DART_IDENTIFIER_STRING, Tone::String},
    {SCE_DART_OPERATOR, Tone::Operator},
    {SCE_DART_OPERATOR_STRING, Tone::Operator},
    {SCE_DART_SYMBOL_IDENTIFIER, Tone::Variable},
    {SCE_DART_SYMBOL_OPERATOR, Tone::Operator},
    {SCE_DART_NUMBER, Tone::Number},
    {SCE_DART_KEY, Tone::Keyword},
    {SCE_DART_METADATA, Tone::Preprocessor},
    {SCE_DART_KW_PRIMARY, Tone::Keyword},
    {SCE_DART_KW_SECONDARY, Tone::Keyword},
    {SCE_DART_KW_TERTIARY, Tone::Keyword},
    {SCE_DART_KW_TYPE, Tone::Type},
    {SCE_DART_STRINGEOL, Tone::String},
};

constexpr StyleSpec kZigStyles[] = {
    {SCE_ZIG_COMMENTLINE, Tone::Comment},
    {SCE_ZIG_COMMENTLINEDOC, Tone::Comment},
    {SCE_ZIG_COMMENTLINETOP, Tone::Comment},
    {SCE_ZIG_NUMBER, Tone::Number},
    {SCE_ZIG_OPERATOR, Tone::Operator},
    {SCE_ZIG_CHARACTER, Tone::String},
    {SCE_ZIG_STRING, Tone::String},
    {SCE_ZIG_MULTISTRING, Tone::String},
    {SCE_ZIG_ESCAPECHAR, Tone::Constant},
    {SCE_ZIG_IDENTIFIER, Tone::Default},
    {SCE_ZIG_FUNCTION, Tone::Function},
    {SCE_ZIG_BUILTIN_FUNCTION, Tone::Function},
    {SCE_ZIG_KW_PRIMARY, Tone::Keyword},
    {SCE_ZIG_KW_SECONDARY, Tone::Keyword},
    {SCE_ZIG_KW_TERTIARY, Tone::Keyword},
    {SCE_ZIG_KW_TYPE, Tone::Type},
    {SCE_ZIG_IDENTIFIER_STRING, Tone::String},
    {SCE_ZIG_STRINGEOL, Tone::String},
};

constexpr StyleSpec kNixStyles[] = {
    {SCE_NIX_COMMENTLINE, Tone::Comment},
    {SCE_NIX_COMMENTBLOCK, Tone::Comment},
    {SCE_NIX_STRING, Tone::String},
    {SCE_NIX_STRING_MULTILINE, Tone::String},
    {SCE_NIX_ESCAPECHAR, Tone::Constant},
    {SCE_NIX_IDENTIFIER, Tone::Variable},
    {SCE_NIX_OPERATOR, Tone::Operator},
    {SCE_NIX_OPERATOR_STRING, Tone::Operator},
    {SCE_NIX_NUMBER, Tone::Number},
    {SCE_NIX_KEY, Tone::Attribute},
    {SCE_NIX_PATH, Tone::String},
    {SCE_NIX_KEYWORD1, Tone::Keyword},
    {SCE_NIX_KEYWORD2, Tone::Keyword},
    {SCE_NIX_KEYWORD3, Tone::Keyword},
    {SCE_NIX_KEYWORD4, Tone::Keyword},
    {SCE_NIX_STRINGEOL, Tone::String},
};

constexpr StyleSpec kVisualBasicStyles[] = {
    {SCE_B_COMMENT, Tone::Comment},
    {SCE_B_COMMENTBLOCK, Tone::Comment},
    {SCE_B_DOCLINE, Tone::Comment},
    {SCE_B_DOCBLOCK, Tone::Comment},
    {SCE_B_DOCKEYWORD, Tone::Comment},
    {SCE_B_NUMBER, Tone::Number},
    {SCE_B_HEXNUMBER, Tone::Number},
    {SCE_B_BINNUMBER, Tone::Number},
    {SCE_B_KEYWORD, Tone::Keyword},
    {SCE_B_KEYWORD2, Tone::Keyword},
    {SCE_B_KEYWORD3, Tone::Keyword},
    {SCE_B_KEYWORD4, Tone::Keyword},
    {SCE_B_STRING, Tone::String},
    {SCE_B_STRINGEOL, Tone::String},
    {SCE_B_PREPROCESSOR, Tone::Preprocessor},
    {SCE_B_OPERATOR, Tone::Operator},
    {SCE_B_IDENTIFIER, Tone::Default},
    {SCE_B_DATE, Tone::Constant},
    {SCE_B_CONSTANT, Tone::Constant},
    {SCE_B_ASM, Tone::Preprocessor},
    {SCE_B_LABEL, Tone::Function},
    {SCE_B_ERROR, Tone::Error},
};

constexpr StyleSpec kPropertiesStyles[] = {
    {SCE_PROPS_COMMENT, Tone::Comment},
    {SCE_PROPS_SECTION, Tone::Header, true},
    {SCE_PROPS_ASSIGNMENT, Tone::Operator},
    {SCE_PROPS_DEFVAL, Tone::String},
    {SCE_PROPS_KEY, Tone::Attribute},
};

constexpr StyleSpec kAssemblyStyles[] = {
    {SCE_ASM_COMMENT, Tone::Comment},
    {SCE_ASM_COMMENTBLOCK, Tone::Comment},
    {SCE_ASM_COMMENTDIRECTIVE, Tone::Comment},
    {SCE_ASM_NUMBER, Tone::Number},
    {SCE_ASM_STRING, Tone::String},
    {SCE_ASM_CHARACTER, Tone::String},
    {SCE_ASM_STRINGEOL, Tone::String},
    {SCE_ASM_STRINGBACKQUOTE, Tone::String},
    {SCE_ASM_OPERATOR, Tone::Operator},
    {SCE_ASM_IDENTIFIER, Tone::Default},
    {SCE_ASM_CPUINSTRUCTION, Tone::Keyword},
    {SCE_ASM_MATHINSTRUCTION, Tone::Keyword},
    {SCE_ASM_EXTINSTRUCTION, Tone::Function},
    {SCE_ASM_REGISTER, Tone::Type},
    {SCE_ASM_DIRECTIVE, Tone::Preprocessor},
    {SCE_ASM_DIRECTIVEOPERAND, Tone::Attribute},
};

void ApplyStyleMap(HWND sci, Language language) {
    switch (language) {
    case Language::Cpp:
    case Language::CSharp:
    case Language::Java:
    case Language::JavaScript:
    case Language::TypeScript:
    case Language::Go:
    case Language::Kotlin:
    case Language::Swift:
    case Language::Scala:
    case Language::ObjectiveC:
        ApplySpecs(sci, kCppStyles);
        break;
    case Language::Python: ApplySpecs(sci, kPythonStyles); break;
    case Language::Html:
    case Language::Xml:
    case Language::Php:
        ApplySpecs(sci, kHtmlStyles);
        break;
    case Language::Css: ApplySpecs(sci, kCssStyles); break;
    case Language::Json: ApplySpecs(sci, kJsonStyles); break;
    case Language::Markdown: ApplySpecs(sci, kMarkdownStyles); break;
    case Language::Bash:
    case Language::Dockerfile:
        ApplySpecs(sci, kBashStyles);
        break;
    case Language::Sql: ApplySpecs(sci, kSqlStyles); break;
    case Language::Rust: ApplySpecs(sci, kRustStyles); break;
    case Language::Ruby: ApplySpecs(sci, kRubyStyles); break;
    case Language::Perl: ApplySpecs(sci, kPerlStyles); break;
    case Language::Lua: ApplySpecs(sci, kLuaStyles); break;
    case Language::Yaml: ApplySpecs(sci, kYamlStyles); break;
    case Language::Toml: ApplySpecs(sci, kTomlStyles); break;
    case Language::Ini: ApplySpecs(sci, kIniStyles); break;
    case Language::Makefile: ApplySpecs(sci, kMakeStyles); break;
    case Language::Batch: ApplySpecs(sci, kBatchStyles); break;
    case Language::PowerShell: ApplySpecs(sci, kPowerShellStyles); break;
    case Language::Diff: ApplySpecs(sci, kDiffStyles); break;
    case Language::CMake: ApplySpecs(sci, kCMakeStyles); break;
    case Language::Latex: ApplySpecs(sci, kLatexStyles); break;
    case Language::R: ApplySpecs(sci, kRStyles); break;
    case Language::Haskell: ApplySpecs(sci, kHaskellStyles); break;
    case Language::Dart: ApplySpecs(sci, kDartStyles); break;
    case Language::Zig: ApplySpecs(sci, kZigStyles); break;
    case Language::Nix: ApplySpecs(sci, kNixStyles); break;
    case Language::VisualBasic: ApplySpecs(sci, kVisualBasicStyles); break;
    case Language::Properties: ApplySpecs(sci, kPropertiesStyles); break;
    case Language::Assembly: ApplySpecs(sci, kAssemblyStyles); break;
    case Language::Plain: break;
    }
}

struct ExtensionDef {
    const wchar_t* extension;
    Language language;
};

constexpr ExtensionDef kExtensions[] = {
    {L"c", Language::Cpp},       {L"h", Language::Cpp},
    {L"cc", Language::Cpp},      {L"cpp", Language::Cpp},
    {L"cxx", Language::Cpp},     {L"c++", Language::Cpp},
    {L"hh", Language::Cpp},      {L"hpp", Language::Cpp},
    {L"hxx", Language::Cpp},     {L"h++", Language::Cpp},
    {L"inl", Language::Cpp},     {L"ipp", Language::Cpp},
    {L"tpp", Language::Cpp},     {L"cs", Language::CSharp},
    {L"csx", Language::CSharp},  {L"java", Language::Java},
    {L"js", Language::JavaScript}, {L"mjs", Language::JavaScript},
    {L"cjs", Language::JavaScript}, {L"jsx", Language::JavaScript},
    {L"ts", Language::TypeScript}, {L"mts", Language::TypeScript},
    {L"cts", Language::TypeScript}, {L"tsx", Language::TypeScript},
    {L"go", Language::Go},       {L"kt", Language::Kotlin},
    {L"kts", Language::Kotlin},  {L"swift", Language::Swift},
    {L"scala", Language::Scala}, {L"sc", Language::Scala},
    {L"m", Language::ObjectiveC}, {L"mm", Language::ObjectiveC},
    {L"py", Language::Python},   {L"pyw", Language::Python},
    {L"pyi", Language::Python},  {L"html", Language::Html},
    {L"htm", Language::Html},    {L"xhtml", Language::Html},
    {L"shtml", Language::Html},  {L"xml", Language::Xml},
    {L"xsd", Language::Xml},     {L"xsl", Language::Xml},
    {L"xslt", Language::Xml},    {L"svg", Language::Xml},
    {L"plist", Language::Xml},   {L"csproj", Language::Xml},
    {L"vbproj", Language::Xml},  {L"fsproj", Language::Xml},
    {L"vcxproj", Language::Xml}, {L"props", Language::Xml},
    {L"targets", Language::Xml}, {L"xaml", Language::Xml},
    {L"config", Language::Xml},  {L"php", Language::Php},
    {L"php3", Language::Php},    {L"php4", Language::Php},
    {L"php5", Language::Php},    {L"phtml", Language::Php},
    {L"css", Language::Css},     {L"scss", Language::Css},
    {L"sass", Language::Css},    {L"less", Language::Css},
    {L"json", Language::Json},   {L"jsonc", Language::Json},
    {L"md", Language::Markdown}, {L"markdown", Language::Markdown},
    {L"mdown", Language::Markdown}, {L"mkdn", Language::Markdown},
    {L"mdx", Language::Markdown}, {L"sh", Language::Bash},
    {L"bash", Language::Bash},   {L"zsh", Language::Bash},
    {L"ksh", Language::Bash},    {L"dockerfile", Language::Dockerfile},
    {L"sql", Language::Sql},
    {L"rs", Language::Rust},     {L"rb", Language::Ruby},
    {L"ruby", Language::Ruby},   {L"rake", Language::Ruby},
    {L"gemspec", Language::Ruby}, {L"pl", Language::Perl},
    {L"pm", Language::Perl},     {L"t", Language::Perl},
    {L"lua", Language::Lua},     {L"yaml", Language::Yaml},
    {L"yml", Language::Yaml},    {L"toml", Language::Toml},
    {L"ini", Language::Ini},     {L"cfg", Language::Ini},
    {L"conf", Language::Ini},    {L"env", Language::Ini},
    {L"mk", Language::Makefile}, {L"mak", Language::Makefile},
    {L"bat", Language::Batch},   {L"cmd", Language::Batch},
    {L"ps1", Language::PowerShell}, {L"psm1", Language::PowerShell},
    {L"psd1", Language::PowerShell}, {L"diff", Language::Diff},
    {L"patch", Language::Diff},  {L"cmake", Language::CMake},
    {L"tex", Language::Latex},   {L"sty", Language::Latex},
    {L"cls", Language::Latex},   {L"bib", Language::Latex},
    {L"r", Language::R},         {L"hs", Language::Haskell},
    {L"lhs", Language::Haskell}, {L"dart", Language::Dart},
    {L"zig", Language::Zig},     {L"nix", Language::Nix},
    {L"vb", Language::VisualBasic}, {L"vbs", Language::VisualBasic},
    {L"bas", Language::VisualBasic}, {L"frm", Language::VisualBasic},
    {L"properties", Language::Properties}, {L"asm", Language::Assembly},
    {L"s", Language::Assembly},
};

struct FilenameDef {
    const wchar_t* name;
    Language language;
};

constexpr FilenameDef kFilenames[] = {
    {L"makefile", Language::Makefile},
    {L"gnumakefile", Language::Makefile},
    {L"bsdmakefile", Language::Makefile},
    {L"cmakelists.txt", Language::CMake},
    {L"dockerfile", Language::Dockerfile},
    {L"gemfile", Language::Ruby},
    {L"rakefile", Language::Ruby},
    {L"vagrantfile", Language::Ruby},
    {L"podfile", Language::Ruby},
    {L"go.mod", Language::Go},
    {L".editorconfig", Language::Ini},
    {L".gitconfig", Language::Ini},
    {L".npmrc", Language::Ini},
    {L".env", Language::Ini},
};

}  // namespace

Language Detect(const std::wstring& path) {
    std::wstring lower = ToLower(path);
    const size_t slash = lower.find_last_of(L"\\/");
    const std::wstring file = slash == std::wstring::npos ? lower : lower.substr(slash + 1);
    if (file.empty()) {
        return Language::Plain;
    }
    for (const FilenameDef& entry : kFilenames) {
        if (file == entry.name) {
            return entry.language;
        }
    }
    const size_t dot = file.find_last_of(L'.');
    if (dot == std::wstring::npos || dot + 1 >= file.size()) {
        return Language::Plain;
    }
    const std::wstring extension = file.substr(dot + 1);
    for (const ExtensionDef& entry : kExtensions) {
        if (extension == entry.extension) {
            return entry.language;
        }
    }
    return Language::Plain;
}

const wchar_t* Name(Language language) {
    switch (language) {
    case Language::Plain: return L"Plain Text";
    case Language::Cpp: return L"C/C++";
    case Language::CSharp: return L"C#";
    case Language::Java: return L"Java";
    case Language::JavaScript: return L"JavaScript";
    case Language::TypeScript: return L"TypeScript";
    case Language::Go: return L"Go";
    case Language::Kotlin: return L"Kotlin";
    case Language::Swift: return L"Swift";
    case Language::Scala: return L"Scala";
    case Language::ObjectiveC: return L"Objective-C";
    case Language::Python: return L"Python";
    case Language::Html: return L"HTML";
    case Language::Xml: return L"XML";
    case Language::Php: return L"PHP";
    case Language::Css: return L"CSS";
    case Language::Json: return L"JSON";
    case Language::Markdown: return L"Markdown";
    case Language::Bash: return L"Shell";
    case Language::Dockerfile: return L"Dockerfile";
    case Language::Sql: return L"SQL";
    case Language::Rust: return L"Rust";
    case Language::Ruby: return L"Ruby";
    case Language::Perl: return L"Perl";
    case Language::Lua: return L"Lua";
    case Language::Yaml: return L"YAML";
    case Language::Toml: return L"TOML";
    case Language::Ini: return L"Config";
    case Language::Makefile: return L"Makefile";
    case Language::Batch: return L"Batch";
    case Language::PowerShell: return L"PowerShell";
    case Language::Diff: return L"Diff";
    case Language::CMake: return L"CMake";
    case Language::Latex: return L"LaTeX";
    case Language::R: return L"R";
    case Language::Haskell: return L"Haskell";
    case Language::Dart: return L"Dart";
    case Language::Zig: return L"Zig";
    case Language::Nix: return L"Nix";
    case Language::VisualBasic: return L"Visual Basic";
    case Language::Properties: return L"Properties";
    case Language::Assembly: return L"Assembly";
    }
    return L"Plain Text";
}

void Apply(HWND sci, Language language, bool enabled, unsigned long long bytes) {
    if (!sci) {
        return;
    }
    if (!enabled || bytes > kMaxHighlightBytes) {
        language = Language::Plain;
    }
    const Lexilla::LexerModule* module = ModuleFor(language);
    Scintilla::ILexer5* lexer = module ? module->Create() : nullptr;
    S(sci, SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lexer));
    S(sci, SCI_STYLECLEARALL);
    if (!module) {
        // Remove styles left by a lexer if this document was already styled.
        const sptr_t length = S(sci, SCI_GETLENGTH);
        if (length > 0 && S(sci, SCI_GETENDSTYLED) > 0) {
            S(sci, SCI_STARTSTYLING, 0);
            S(sci, SCI_SETSTYLING, static_cast<uptr_t>(length), 0);
        }
        return;
    }
    SetKeywords(sci, language);
    ApplyStyleMap(sci, language);
    S(sci, SCI_COLOURISE, 0, -1);
}

}  // namespace syntax
