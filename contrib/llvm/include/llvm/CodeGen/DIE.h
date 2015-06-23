//===--- lib/CodeGen/DIE.h - DWARF Info Entries -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Data structures for DWARF info entries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_ASMPRINTER_DIE_H
#define LLVM_LIB_CODEGEN_ASMPRINTER_DIE_H

#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/DwarfStringPoolEntry.h"
#include "llvm/Support/Dwarf.h"
#include <vector>

namespace llvm {
class AsmPrinter;
class MCExpr;
class MCSymbol;
class raw_ostream;
class DwarfTypeUnit;

//===--------------------------------------------------------------------===//
/// DIEAbbrevData - Dwarf abbreviation data, describes one attribute of a
/// Dwarf abbreviation.
class DIEAbbrevData {
  /// Attribute - Dwarf attribute code.
  ///
  dwarf::Attribute Attribute;

  /// Form - Dwarf form code.
  ///
  dwarf::Form Form;

public:
  DIEAbbrevData(dwarf::Attribute A, dwarf::Form F) : Attribute(A), Form(F) {}

  // Accessors.
  dwarf::Attribute getAttribute() const { return Attribute; }
  dwarf::Form getForm() const { return Form; }

  /// Profile - Used to gather unique data for the abbreviation folding set.
  ///
  void Profile(FoldingSetNodeID &ID) const;
};

//===--------------------------------------------------------------------===//
/// DIEAbbrev - Dwarf abbreviation, describes the organization of a debug
/// information object.
class DIEAbbrev : public FoldingSetNode {
  /// Unique number for node.
  ///
  unsigned Number;

  /// Tag - Dwarf tag code.
  ///
  dwarf::Tag Tag;

  /// Children - Whether or not this node has children.
  ///
  // This cheats a bit in all of the uses since the values in the standard
  // are 0 and 1 for no children and children respectively.
  bool Children;

  /// Data - Raw data bytes for abbreviation.
  ///
  SmallVector<DIEAbbrevData, 12> Data;

public:
  DIEAbbrev(dwarf::Tag T, bool C) : Tag(T), Children(C), Data() {}

  // Accessors.
  dwarf::Tag getTag() const { return Tag; }
  unsigned getNumber() const { return Number; }
  bool hasChildren() const { return Children; }
  const SmallVectorImpl<DIEAbbrevData> &getData() const { return Data; }
  void setChildrenFlag(bool hasChild) { Children = hasChild; }
  void setNumber(unsigned N) { Number = N; }

  /// AddAttribute - Adds another set of attribute information to the
  /// abbreviation.
  void AddAttribute(dwarf::Attribute Attribute, dwarf::Form Form) {
    Data.push_back(DIEAbbrevData(Attribute, Form));
  }

  /// Profile - Used to gather unique data for the abbreviation folding set.
  ///
  void Profile(FoldingSetNodeID &ID) const;

  /// Emit - Print the abbreviation using the specified asm printer.
  ///
  void Emit(const AsmPrinter *AP) const;

#ifndef NDEBUG
  void print(raw_ostream &O);
  void dump();
#endif
};

//===--------------------------------------------------------------------===//
/// DIEInteger - An integer value DIE.
///
class DIEInteger {
  uint64_t Integer;

public:
  explicit DIEInteger(uint64_t I) : Integer(I) {}

  /// BestForm - Choose the best form for integer.
  ///
  static dwarf::Form BestForm(bool IsSigned, uint64_t Int) {
    if (IsSigned) {
      const int64_t SignedInt = Int;
      if ((char)Int == SignedInt)
        return dwarf::DW_FORM_data1;
      if ((short)Int == SignedInt)
        return dwarf::DW_FORM_data2;
      if ((int)Int == SignedInt)
        return dwarf::DW_FORM_data4;
    } else {
      if ((unsigned char)Int == Int)
        return dwarf::DW_FORM_data1;
      if ((unsigned short)Int == Int)
        return dwarf::DW_FORM_data2;
      if ((unsigned int)Int == Int)
        return dwarf::DW_FORM_data4;
    }
    return dwarf::DW_FORM_data8;
  }

  uint64_t getValue() const { return Integer; }
  void setValue(uint64_t Val) { Integer = Val; }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEExpr - An expression DIE.
//
class DIEExpr {
  const MCExpr *Expr;

public:
  explicit DIEExpr(const MCExpr *E) : Expr(E) {}

  /// getValue - Get MCExpr.
  ///
  const MCExpr *getValue() const { return Expr; }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIELabel - A label DIE.
//
class DIELabel {
  const MCSymbol *Label;

public:
  explicit DIELabel(const MCSymbol *L) : Label(L) {}

  /// getValue - Get MCSymbol.
  ///
  const MCSymbol *getValue() const { return Label; }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEDelta - A simple label difference DIE.
///
class DIEDelta {
  const MCSymbol *LabelHi;
  const MCSymbol *LabelLo;

public:
  DIEDelta(const MCSymbol *Hi, const MCSymbol *Lo) : LabelHi(Hi), LabelLo(Lo) {}

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEString - A container for string values.
///
class DIEString {
  DwarfStringPoolEntryRef S;

public:
  DIEString(DwarfStringPoolEntryRef S) : S(S) {}

  /// getString - Grab the string out of the object.
  StringRef getString() const { return S.getString(); }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEEntry - A pointer to another debug information entry.  An instance of
/// this class can also be used as a proxy for a debug information entry not
/// yet defined (ie. types.)
class DIE;
class DIEEntry {
  DIE *Entry;

  DIEEntry() = delete;

public:
  explicit DIEEntry(DIE &E) : Entry(&E) {}

  DIE &getEntry() const { return *Entry; }

  /// Returns size of a ref_addr entry.
  static unsigned getRefAddrSize(const AsmPrinter *AP);

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
    return Form == dwarf::DW_FORM_ref_addr ? getRefAddrSize(AP)
                                           : sizeof(int32_t);
  }

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// \brief A signature reference to a type unit.
class DIETypeSignature {
  const DwarfTypeUnit *Unit;

  DIETypeSignature() = delete;

public:
  explicit DIETypeSignature(const DwarfTypeUnit &Unit) : Unit(&Unit) {}

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const {
    assert(Form == dwarf::DW_FORM_ref_sig8);
    return 8;
  }

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIELocList - Represents a pointer to a location list in the debug_loc
/// section.
//
class DIELocList {
  // Index into the .debug_loc vector.
  size_t Index;

public:
  DIELocList(size_t I) : Index(I) {}

  /// getValue - Grab the current index out.
  size_t getValue() const { return Index; }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEValue - A debug information entry value. Some of these roughly correlate
/// to DWARF attribute classes.
///
class DIEBlock;
class DIELoc;
class DIEValue {
public:
  enum Type {
    isNone,
#define HANDLE_DIEVALUE(T) is##T,
#include "llvm/CodeGen/DIEValue.def"
  };

private:
  /// Ty - Type of data stored in the value.
  ///
  Type Ty = isNone;
  dwarf::Attribute Attribute = (dwarf::Attribute)0;
  dwarf::Form Form = (dwarf::Form)0;

  /// Storage for the value.
  ///
  /// All values that aren't standard layout (or are larger than 8 bytes)
  /// should be stored by reference instead of by value.
  typedef AlignedCharArrayUnion<DIEInteger, DIEString, DIEExpr, DIELabel,
                                DIEDelta *, DIEEntry, DIETypeSignature,
                                DIEBlock *, DIELoc *, DIELocList> ValTy;
  static_assert(sizeof(ValTy) <= sizeof(uint64_t) ||
                    sizeof(ValTy) <= sizeof(void *),
                "Expected all large types to be stored via pointer");

  /// Underlying stored value.
  ValTy Val;

  template <class T> void construct(T V) {
    static_assert(std::is_standard_layout<T>::value ||
                      std::is_pointer<T>::value,
                  "Expected standard layout or pointer");
    new (reinterpret_cast<void *>(Val.buffer)) T(V);
  }

  template <class T> T *get() { return reinterpret_cast<T *>(Val.buffer); }
  template <class T> const T *get() const {
    return reinterpret_cast<const T *>(Val.buffer);
  }
  template <class T> void destruct() { get<T>()->~T(); }

  /// Destroy the underlying value.
  ///
  /// This should get optimized down to a no-op.  We could skip it if we could
  /// add a static assert on \a std::is_trivially_copyable(), but we currently
  /// support versions of GCC that don't understand that.
  void destroyVal() {
    switch (Ty) {
    case isNone:
      return;
#define HANDLE_DIEVALUE_SMALL(T)                                               \
  case is##T:                                                                  \
    destruct<DIE##T>();
    return;
#define HANDLE_DIEVALUE_LARGE(T)                                               \
  case is##T:                                                                  \
    destruct<const DIE##T *>();
    return;
#include "llvm/CodeGen/DIEValue.def"
    }
  }

  /// Copy the underlying value.
  ///
  /// This should get optimized down to a simple copy.  We need to actually
  /// construct the value, rather than calling memcpy, to satisfy strict
  /// aliasing rules.
  void copyVal(const DIEValue &X) {
    switch (Ty) {
    case isNone:
      return;
#define HANDLE_DIEVALUE_SMALL(T)                                               \
  case is##T:                                                                  \
    construct<DIE##T>(*X.get<DIE##T>());                                       \
    return;
#define HANDLE_DIEVALUE_LARGE(T)                                               \
  case is##T:                                                                  \
    construct<const DIE##T *>(*X.get<const DIE##T *>());                       \
    return;
#include "llvm/CodeGen/DIEValue.def"
    }
  }

public:
  DIEValue() = default;
  DIEValue(const DIEValue &X) : Ty(X.Ty), Attribute(X.Attribute), Form(X.Form) {
    copyVal(X);
  }
  DIEValue &operator=(const DIEValue &X) {
    destroyVal();
    Ty = X.Ty;
    Attribute = X.Attribute;
    Form = X.Form;
    copyVal(X);
    return *this;
  }
  ~DIEValue() { destroyVal(); }

#define HANDLE_DIEVALUE_SMALL(T)                                               \
  DIEValue(dwarf::Attribute Attribute, dwarf::Form Form, const DIE##T &V)      \
      : Ty(is##T), Attribute(Attribute), Form(Form) {                          \
    construct<DIE##T>(V);                                                      \
  }
#define HANDLE_DIEVALUE_LARGE(T)                                               \
  DIEValue(dwarf::Attribute Attribute, dwarf::Form Form, const DIE##T *V)      \
      : Ty(is##T), Attribute(Attribute), Form(Form) {                          \
    assert(V && "Expected valid value");                                       \
    construct<const DIE##T *>(V);                                              \
  }
#include "llvm/CodeGen/DIEValue.def"

  // Accessors
  Type getType() const { return Ty; }
  dwarf::Attribute getAttribute() const { return Attribute; }
  dwarf::Form getForm() const { return Form; }
  explicit operator bool() const { return Ty; }

#define HANDLE_DIEVALUE_SMALL(T)                                               \
  const DIE##T &getDIE##T() const {                                            \
    assert(getType() == is##T && "Expected " #T);                              \
    return *get<DIE##T>();                                                     \
  }
#define HANDLE_DIEVALUE_LARGE(T)                                               \
  const DIE##T &getDIE##T() const {                                            \
    assert(getType() == is##T && "Expected " #T);                              \
    return **get<const DIE##T *>();                                            \
  }
#include "llvm/CodeGen/DIEValue.def"

  /// EmitValue - Emit value via the Dwarf writer.
  ///
  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;

  /// SizeOf - Return the size of a value in bytes.
  ///
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
  void dump() const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIE - A structured debug information entry.  Has an abbreviation which
/// describes its organization.
class DIE {
protected:
  /// Offset - Offset in debug info section.
  ///
  unsigned Offset;

  /// Size - Size of instance + children.
  ///
  unsigned Size;

  unsigned AbbrevNumber = ~0u;

  /// Tag - Dwarf tag code.
  ///
  dwarf::Tag Tag = (dwarf::Tag)0;

  /// Children DIEs.
  ///
  // This can't be a vector<DIE> because pointer validity is requirent for the
  // Parent pointer and DIEEntry.
  // It can't be a list<DIE> because some clients need pointer validity before
  // the object has been added to any child list
  // (eg: DwarfUnit::constructVariableDIE). These aren't insurmountable, but may
  // be more convoluted than beneficial.
  std::vector<std::unique_ptr<DIE>> Children;

  DIE *Parent;

  /// Attribute values.
  ///
  SmallVector<DIEValue, 12> Values;

protected:
  DIE() : Offset(0), Size(0), Parent(nullptr) {}

public:
  explicit DIE(dwarf::Tag Tag)
      : Offset(0), Size(0), Tag(Tag), Parent(nullptr) {}

  // Accessors.
  unsigned getAbbrevNumber() const { return AbbrevNumber; }
  dwarf::Tag getTag() const { return Tag; }
  unsigned getOffset() const { return Offset; }
  unsigned getSize() const { return Size; }
  bool hasChildren() const { return !Children.empty(); }

  typedef std::vector<std::unique_ptr<DIE>>::const_iterator child_iterator;
  typedef iterator_range<child_iterator> child_range;

  child_range children() const {
    return llvm::make_range(Children.begin(), Children.end());
  }

  typedef SmallVectorImpl<DIEValue>::const_iterator value_iterator;
  typedef iterator_range<value_iterator> value_range;

  value_iterator values_begin() const { return Values.begin(); }
  value_iterator values_end() const { return Values.end(); }
  value_range values() const {
    return llvm::make_range(values_begin(), values_end());
  }

  void setValue(unsigned I, DIEValue New) {
    assert(I < Values.size());
    Values[I] = New;
  }
  DIE *getParent() const { return Parent; }

  /// Generate the abbreviation for this DIE.
  ///
  /// Calculate the abbreviation for this, which should be uniqued and
  /// eventually used to call \a setAbbrevNumber().
  DIEAbbrev generateAbbrev() const;

  /// Set the abbreviation number for this DIE.
  void setAbbrevNumber(unsigned I) { AbbrevNumber = I; }

  /// Climb up the parent chain to get the compile or type unit DIE this DIE
  /// belongs to.
  const DIE *getUnit() const;
  /// Similar to getUnit, returns null when DIE is not added to an
  /// owner yet.
  const DIE *getUnitOrNull() const;
  void setOffset(unsigned O) { Offset = O; }
  void setSize(unsigned S) { Size = S; }

  /// addValue - Add a value and attributes to a DIE.
  ///
  void addValue(DIEValue Value) { Values.push_back(Value); }
  template <class T>
  void addValue(dwarf::Attribute Attribute, dwarf::Form Form, T &&Value) {
    Values.emplace_back(Attribute, Form, std::forward<T>(Value));
  }

  /// addChild - Add a child to the DIE.
  ///
  DIE &addChild(std::unique_ptr<DIE> Child) {
    assert(!Child->getParent());
    Child->Parent = this;
    Children.push_back(std::move(Child));
    return *Children.back();
  }

  /// Find a value in the DIE with the attribute given.
  ///
  /// Returns a default-constructed DIEValue (where \a DIEValue::getType()
  /// gives \a DIEValue::isNone) if no such attribute exists.
  DIEValue findAttribute(dwarf::Attribute Attribute) const;

#ifndef NDEBUG
  void print(raw_ostream &O, unsigned IndentCount = 0) const;
  void dump();
#endif
};

//===--------------------------------------------------------------------===//
/// DIELoc - Represents an expression location.
//
class DIELoc : public DIE {
  mutable unsigned Size; // Size in bytes excluding size header.

public:
  DIELoc() : Size(0) {}

  /// ComputeSize - Calculate the size of the location expression.
  ///
  unsigned ComputeSize(const AsmPrinter *AP) const;

  /// BestForm - Choose the best form for data.
  ///
  dwarf::Form BestForm(unsigned DwarfVersion) const {
    if (DwarfVersion > 3)
      return dwarf::DW_FORM_exprloc;
    // Pre-DWARF4 location expressions were blocks and not exprloc.
    if ((unsigned char)Size == Size)
      return dwarf::DW_FORM_block1;
    if ((unsigned short)Size == Size)
      return dwarf::DW_FORM_block2;
    if ((unsigned int)Size == Size)
      return dwarf::DW_FORM_block4;
    return dwarf::DW_FORM_block;
  }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

//===--------------------------------------------------------------------===//
/// DIEBlock - Represents a block of values.
//
class DIEBlock : public DIE {
  mutable unsigned Size; // Size in bytes excluding size header.

public:
  DIEBlock() : Size(0) {}

  /// ComputeSize - Calculate the size of the location expression.
  ///
  unsigned ComputeSize(const AsmPrinter *AP) const;

  /// BestForm - Choose the best form for data.
  ///
  dwarf::Form BestForm() const {
    if ((unsigned char)Size == Size)
      return dwarf::DW_FORM_block1;
    if ((unsigned short)Size == Size)
      return dwarf::DW_FORM_block2;
    if ((unsigned int)Size == Size)
      return dwarf::DW_FORM_block4;
    return dwarf::DW_FORM_block;
  }

  void EmitValue(const AsmPrinter *AP, dwarf::Form Form) const;
  unsigned SizeOf(const AsmPrinter *AP, dwarf::Form Form) const;

#ifndef NDEBUG
  void print(raw_ostream &O) const;
#endif
};

} // namespace llvm

#endif
