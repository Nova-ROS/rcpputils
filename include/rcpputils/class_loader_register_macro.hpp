// Copyright 2026 Nova ROS, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RCPPUTILS__CLASS_LOADER_REGISTER_MACRO_HPP_
#define RCPPUTILS__CLASS_LOADER_REGISTER_MACRO_HPP_

#include "rcpputils/class_loader.hpp"

#define RCPPUTILS_CONCAT_IMPL(a, b) a ## b
#define RCPPUTILS_CONCAT(a, b) RCPPUTILS_CONCAT_IMPL(a, b)

// Helper to prevent optimization of static initializers
#ifdef __GNUC__
  #define RCPPUTILS_USED_ATTRIBUTE __attribute__((used))
#else
  #define RCPPUTILS_USED_ATTRIBUTE
#endif

// Use a struct with a constructor to trigger registration at library load time
// The __attribute__((used)) prevents the linker from optimizing away the static variable
#define CLASS_LOADER_REGISTER_CLASS_INTERNAL(Derived, Base, UniqueID) \
struct RCPPUTILS_CONCAT(ClassRegistrar_, UniqueID) { \
  RCPPUTILS_CONCAT(ClassRegistrar_, UniqueID)() { \
    rcpputils::class_loader::register_class<Base>( \
      #Derived, \
      []() -> std::unique_ptr<Base> { return std::make_unique<Derived>(); }, \
      [](Base * obj) { delete obj; } \
    ); \
  } \
}; \
static RCPPUTILS_CONCAT(ClassRegistrar_, UniqueID) RCPPUTILS_CONCAT(g_class_registrar_, UniqueID) RCPPUTILS_USED_ATTRIBUTE;

/**
 * \brief Register a derived class with the class loader factory.
 *
 * This macro should be placed in the .cpp file of the derived class.
 * It automatically registers the class when the shared library is loaded.
 *
 * Usage:
 * \code{.cpp}
 * #include "rcpputils/class_loader_register_macro.hpp"
 *
 * class MyPlugin : public PluginBase {
 *   // ...
 * };
 *
 * CLASS_LOADER_REGISTER_CLASS(MyPlugin, PluginBase)
 * \endcode
 *
 * \param Derived The derived class name (must have a default constructor).
 * \param Base The base class name.
 */
#define CLASS_LOADER_REGISTER_CLASS(Derived, Base) \
  CLASS_LOADER_REGISTER_CLASS_INTERNAL(Derived, Base, __COUNTER__)

/**
 * \brief Register a derived class with a custom factory function.
 *
 * Use this when the derived class doesn't have a default constructor
 * or requires special construction logic.
 *
 * \param Derived The derived class name.
 * \param Base The base class name.
 * \param FactoryFunc A callable that returns std::unique_ptr<Base>.
 */
#define CLASS_LOADER_REGISTER_CLASS_WITH_FACTORY(Derived, Base, FactoryFunc) \
struct RCPPUTILS_CONCAT(ClassRegistrarFactory_, __COUNTER__) { \
  RCPPUTILS_CONCAT(ClassRegistrarFactory_, __COUNTER__)() { \
    rcpputils::class_loader::register_class<Base>( \
      #Derived, \
      FactoryFunc, \
      [](Base * obj) { delete obj; } \
    ); \
  } \
}; \
static RCPPUTILS_CONCAT(ClassRegistrarFactory_, __COUNTER__) \
  RCPPUTILS_CONCAT(g_class_registrar_factory_, __COUNTER__) RCPPUTILS_USED_ATTRIBUTE;

#endif  // RCPPUTILS__CLASS_LOADER_REGISTER_MACRO_HPP_
