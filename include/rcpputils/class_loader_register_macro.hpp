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
#include "rcpputils/visibility_control.hpp"

/**
 * \brief Macro to register a derived class with the factory.
 * \param Derived The derived class name.
 * \param Base The base class name.
 */
#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Base, UniqueID, Message) \
static void rcpputils_auto_register_plugins ## UniqueID() { \
  rcpputils::class_loader::register_class<Base>(#Derived, \
                                      []() -> Base* { return new Derived; }, \
                                      [](Base * obj) { delete obj; }); \
}

#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_HOP1_WITH_MESSAGE(Derived, Base, UniqueID, Message) \
  CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Base, UniqueID, Message)

/**
* @macro This macro is same as CLASS_LOADER_REGISTER_CLASS, but will spit out a message when the plugin is registered
* at library load time
*/
#define CLASS_LOADER_REGISTER_CLASS_WITH_MESSAGE(Derived, Base, Message) \
  CLASS_LOADER_REGISTER_CLASS_INTERNAL_HOP1_WITH_MESSAGE(Derived, Base, __COUNTER__, Message)

/**
* @macro This is the macro which must be declared within the source (.cpp) file for each class that is to be exported as plugin.
* The macro utilizes a trick where a new struct is generated along with a declaration of static global variable of same type after it. The struct's constructor invokes a registration function with the plugin system. When the plugin system loads a library with registered classes in it, the initialization of static variables forces the invocation of the struct constructors, and all exported classes are automatically registerd.
*/
#define CLASS_LOADER_REGISTER_CLASS(Derived, Base) \
  CLASS_LOADER_REGISTER_CLASS_WITH_MESSAGE(Derived, Base, "")

#endif  // RCPPUTILS__CLASS_LOADER_REGISTER_MACRO_HPP_
