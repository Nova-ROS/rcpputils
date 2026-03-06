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

#include "plugin_base.hpp"
#include "rcpputils/class_loader_register_macro.hpp"

namespace test_plugins
{

class PluginA : public PluginBase
{
public:
  std::string get_name() const override
  {
    return "PluginA";
  }

  int calculate(int a, int b) const override
  {
    return a + b;
  }
};

class PluginB : public PluginBase
{
public:
  std::string get_name() const override
  {
    return "PluginB";
  }

  int calculate(int a, int b) const override
  {
    return a * b;
  }
};

class AdvancedPluginImpl : public AdvancedPluginBase
{
public:
  AdvancedPluginImpl()
  : initialized_(false)
  {
  }

  std::string get_description() const override
  {
    return "AdvancedPluginImpl";
  }

  void initialize() override
  {
    initialized_ = true;
  }

  bool is_initialized() const override
  {
    return initialized_;
  }

private:
  bool initialized_;
};

}  // namespace test_plugins

// Register plugins
CLASS_LOADER_REGISTER_CLASS(test_plugins::PluginA, test_plugins::PluginBase)
CLASS_LOADER_REGISTER_CLASS(test_plugins::PluginB, test_plugins::PluginBase)
CLASS_LOADER_REGISTER_CLASS(test_plugins::AdvancedPluginImpl, test_plugins::AdvancedPluginBase)
