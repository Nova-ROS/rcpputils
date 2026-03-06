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

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rcpputils/class_loader.hpp"
#include "test_class_loader_plugins/plugin_base.hpp"

using rcpputils::class_loader::ClassLoader;
using rcpputils::class_loader::ClassLoaderException;
using rcpputils::class_loader::is_class_registered;
using rcpputils::class_loader::register_class;
using rcpputils::class_loader::unregister_class;
using test_plugins::AdvancedPluginBase;
using test_plugins::PluginBase;

// Helper: a simple plugin type local to this test
class LocalDummyPlugin : public PluginBase
{
public:
  std::string get_name() const override {return "Dummy";}
  int calculate(int, int) const override {return 0;}
};

// Test fixture for class loader tests
class ClassLoaderTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    const char * env_val = std::getenv("TEST_CLASS_LOADER_PLUGINS_LIBRARY");
    if (env_val) {
      library_path_ = env_val;
    } else {
      library_path_ = "./libtest_class_loader_plugins.so";
    }
  }

  std::string library_path_;
};

// ---- Plugin registration tests (depend on libtest_class_loader_plugins.so) ----

// Test basic class registration and creation
TEST_F(ClassLoaderTest, BasicClassRegistration)
{
  ClassLoader loader(library_path_);
  ASSERT_TRUE(loader.is_library_loaded());

  // Check available classes
  auto classes = loader.get_available_classes<PluginBase>();
  ASSERT_EQ(classes.size(), 2u);

  // Verify both plugins are registered
  EXPECT_TRUE(is_class_registered<PluginBase>("test_plugins::PluginA"));
  EXPECT_TRUE(is_class_registered<PluginBase>("test_plugins::PluginB"));

  // Create instances using shared_ptr
  auto plugin_a = loader.create_shared_instance<PluginBase>("test_plugins::PluginA");
  ASSERT_NE(plugin_a, nullptr);
  EXPECT_EQ(plugin_a->get_name(), "PluginA");
  EXPECT_EQ(plugin_a->calculate(2, 3), 5);  // PluginA does addition

  auto plugin_b = loader.create_shared_instance<PluginBase>("test_plugins::PluginB");
  ASSERT_NE(plugin_b, nullptr);
  EXPECT_EQ(plugin_b->get_name(), "PluginB");
  EXPECT_EQ(plugin_b->calculate(2, 3), 6);  // PluginB does multiplication
}

// Test unique_ptr creation with custom deleter
TEST_F(ClassLoaderTest, UniquePtrCreation)
{
  ClassLoader loader(library_path_);
  ASSERT_TRUE(loader.is_library_loaded());

  auto plugin = loader.create_unique_instance<PluginBase>("test_plugins::PluginA");
  ASSERT_NE(plugin, nullptr);
  EXPECT_EQ(plugin->get_name(), "PluginA");
  EXPECT_EQ(plugin->calculate(10, 20), 30);
}

// Test creating unregistered class
TEST_F(ClassLoaderTest, CreateUnregisteredClass)
{
  ClassLoader loader(library_path_);
  ASSERT_TRUE(loader.is_library_loaded());

  EXPECT_THROW(
  {
    loader.create_shared_instance<PluginBase>("NonExistentPlugin");
  },
    ClassLoaderException);
}

// Test different base types
TEST_F(ClassLoaderTest, DifferentBaseTypes)
{
  ClassLoader loader(library_path_);
  ASSERT_TRUE(loader.is_library_loaded());

  // PluginBase classes
  auto plugin_classes = loader.get_available_classes<PluginBase>();
  EXPECT_EQ(plugin_classes.size(), 2u);

  // AdvancedPluginBase classes
  auto advanced_classes = loader.get_available_classes<AdvancedPluginBase>();
  EXPECT_EQ(advanced_classes.size(), 1u);
  EXPECT_TRUE(is_class_registered<AdvancedPluginBase>("test_plugins::AdvancedPluginImpl"));

  // Create advanced plugin and exercise its interface
  auto advanced =
    loader.create_shared_instance<AdvancedPluginBase>("test_plugins::AdvancedPluginImpl");
  ASSERT_NE(advanced, nullptr);
  EXPECT_EQ(advanced->get_description(), "AdvancedPluginImpl");
  EXPECT_FALSE(advanced->is_initialized());
  advanced->initialize();
  EXPECT_TRUE(advanced->is_initialized());
}

// Test multiple loaders for same library
TEST_F(ClassLoaderTest, MultipleLoaders)
{
  ClassLoader loader1(library_path_);
  ClassLoader loader2(library_path_);

  EXPECT_TRUE(loader1.is_library_loaded());
  EXPECT_TRUE(loader2.is_library_loaded());

  auto plugin1 = loader1.create_shared_instance<PluginBase>("test_plugins::PluginA");
  auto plugin2 = loader2.create_shared_instance<PluginBase>("test_plugins::PluginA");

  EXPECT_NE(plugin1, nullptr);
  EXPECT_NE(plugin2, nullptr);
  // Different instances
  EXPECT_NE(plugin1.get(), plugin2.get());
}

// ---- Tests that do NOT depend on the plugin library ----

// Test library loading failure
TEST_F(ClassLoaderTest, LibraryLoadFailure)
{
  EXPECT_THROW(
  {
    ClassLoader loader("/nonexistent/path/to/library.so");
  },
    ClassLoaderException);
}

// Test manual class registration / unregistration
TEST_F(ClassLoaderTest, ManualClassRegistration)
{
  const std::string name = "ManualTestPlugin";
  EXPECT_FALSE(is_class_registered<PluginBase>(name));

  register_class<PluginBase>(
    name,
    []() -> std::unique_ptr<PluginBase> {return std::make_unique<LocalDummyPlugin>();},
    [](PluginBase * obj) {delete obj;}
  );

  EXPECT_TRUE(is_class_registered<PluginBase>(name));

  unregister_class<PluginBase>(name);
  EXPECT_FALSE(is_class_registered<PluginBase>(name));
}

// Test library path getter
TEST_F(ClassLoaderTest, GetLibraryPath)
{
  ClassLoader loader(library_path_);
  EXPECT_EQ(loader.get_library_path(), library_path_);
}

// Test move semantics
TEST_F(ClassLoaderTest, MoveSemantics)
{
  ClassLoader loader1(library_path_);
  ASSERT_TRUE(loader1.is_library_loaded());

  // Move constructor
  ClassLoader loader2(std::move(loader1));
  EXPECT_FALSE(loader1.is_library_loaded());  // NOLINT: testing moved-from state
  EXPECT_TRUE(loader2.is_library_loaded());

  // Move assignment
  ClassLoader loader3(library_path_);
  ASSERT_TRUE(loader3.is_library_loaded());
  loader3 = std::move(loader2);
  EXPECT_FALSE(loader2.is_library_loaded());  // NOLINT: testing moved-from state
  EXPECT_TRUE(loader3.is_library_loaded());
}

// Test exception messages contain useful context
TEST_F(ClassLoaderTest, ExceptionMessages)
{
  // Load failure message
  try {
    ClassLoader loader("/nonexistent/library.so");
    FAIL() << "Expected ClassLoaderException";
  } catch (const ClassLoaderException & e) {
    std::string msg = e.what();
    EXPECT_NE(msg.find("Failed to load"), std::string::npos);
    EXPECT_NE(msg.find("/nonexistent/library.so"), std::string::npos);
  }

  // Unregistered class message
  ClassLoader loader(library_path_);
  try {
    loader.create_shared_instance<PluginBase>("NonExistent");
    FAIL() << "Expected ClassLoaderException";
  } catch (const ClassLoaderException & e) {
    std::string msg = e.what();
    EXPECT_NE(msg.find("Class not registered"), std::string::npos);
    EXPECT_NE(msg.find("NonExistent"), std::string::npos);
  }
}

// Test thread safety with concurrent registrations
TEST_F(ClassLoaderTest, ThreadSafety)
{
  constexpr int num_threads = 8;
  constexpr int registrations_per_thread = 50;

  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([i, &success_count]() {
        for (int j = 0; j < registrations_per_thread; ++j) {
          std::string class_name =
            "Thread" + std::to_string(i) + "_Class" + std::to_string(j);
          register_class<PluginBase>(
            class_name,
            []() -> std::unique_ptr<PluginBase> {
              return std::make_unique<LocalDummyPlugin>();
            },
            [](PluginBase * obj) {delete obj;}
          );
          success_count++;
        }
      });
  }

  for (auto & t : threads) {
    t.join();
  }

  EXPECT_EQ(success_count.load(), num_threads * registrations_per_thread);

  // Clean up all registered dummy classes
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < registrations_per_thread; ++j) {
      std::string class_name =
        "Thread" + std::to_string(i) + "_Class" + std::to_string(j);
      unregister_class<PluginBase>(class_name);
    }
  }
}
