#pragma once
#include <vector>
#include "NRLOpMode.h"

// ============================================================
//  OpModeType — used by REGISTER_OPMODE and the ADVERTISE packet
// ============================================================
enum class OpModeType { TELEOP, AUTO };


// ============================================================
//  OpModeEntry — one registered OpMode
// ============================================================
//  factory: creates a fresh instance each time the OpMode is started.
//  Fresh instance = clean member variable state every run.
struct OpModeEntry {
    const char*    name;
    OpModeType     type;
    bool           isExample;  // true = built-in example, false = student code
    int            order;      // lower = shown first; default 1000 (registration order)
    NRLOpMode*   (*factory)();
};


// ============================================================
//  OpModeRegistry — global list of all registered OpModes
// ============================================================
//  Uses a static local vector (Meyer's singleton) to avoid the
//  static initialisation order fiasco across translation units.
class OpModeRegistry {
public:
    static std::vector<OpModeEntry>& all() {
        static std::vector<OpModeEntry> _list;
        return _list;
    }

    static void add(const char* name, OpModeType type, bool isExample,
                    NRLOpMode* (*factory)(), int order = 1000) {
        all().push_back({ name, type, isExample, order, factory });
    }
};


// ============================================================
//  OpModeRegistrar<T> — registers one OpMode via its constructor
// ============================================================
//  C++ guarantees static objects are constructed before setup().
//  So every REGISTER_OPMODE call runs at startup, before anything else.
template<typename T>
class OpModeRegistrar {
public:
    OpModeRegistrar(const char* name, OpModeType type, bool isExample = false,
                    int order = 1000) {
        // ABI guard: references a version-named symbol only a matching .a
        // defines (see NRL_ABI_VERSION in NRLOpMode.h). Runs at static-init
        // time, same as OpModeRegistry::add() below, so it can never be
        // linker-GC'd away.
        NRL_ABI_MARKER();
        OpModeRegistry::add(name, type, isExample,
                            []() -> NRLOpMode* { return new T(); }, order);
    }
};


// ============================================================
//  REGISTER_OPMODE    — used internally by _impl/ wrappers
//  REGISTER_OPMODE_EX — used by built-in example files
// ============================================================
//
//  _reg_##ClassName makes the variable name unique per class
//  so multiple .cpp files don't clash with each other.
#define REGISTER_OPMODE(ClassName, DisplayName, Type) \
    static OpModeRegistrar<ClassName> _reg_##ClassName(DisplayName, OpModeType::Type, false)

#define REGISTER_OPMODE_EX(ClassName, DisplayName, Type) \
    static OpModeRegistrar<ClassName> _reg_##ClassName(DisplayName, OpModeType::Type, true)

//  REGISTER_OPMODE_EX_AT — like REGISTER_OPMODE_EX but pins the menu slot.
//  Order is a small integer: lower numbers appear first in the EXAMPLES menu.
//  Only the four curriculum examples use this; everything else stays at the
//  default order (1000) and is filtered out of the menu unless re-enabled.
#define REGISTER_OPMODE_EX_AT(ClassName, DisplayName, Type, Order) \
    static OpModeRegistrar<ClassName> _reg_##ClassName(DisplayName, OpModeType::Type, true, Order)
