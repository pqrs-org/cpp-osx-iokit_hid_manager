#include <IOKit/hid/IOHIDUsageTables.h>
#include <csignal>
#include <pqrs/osx/iokit_hid_device.hpp>
#include <pqrs/osx/iokit_hid_manager.hpp>

namespace {
auto global_wait = pqrs::make_thread_wait();
}

int main(void) {
  std::signal(SIGINT, [](int) {
    global_wait->notify();
  });

  auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
  auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);
  auto run_loop_thread = std::make_shared<pqrs::cf::run_loop_thread>();

  std::vector<pqrs::cf::cf_ptr<CFDictionaryRef>> matching_dictionaries{
      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::hid::usage_page::generic_desktop,
          pqrs::hid::usage::generic_desktop::keyboard),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::hid::usage_page::generic_desktop,
          pqrs::hid::usage::generic_desktop::mouse),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::hid::usage_page::generic_desktop,
          pqrs::hid::usage::generic_desktop::pointer),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::hid::usage_page::apple_vendor),
  };

  auto hid_manager = std::make_shared<pqrs::osx::iokit_hid_manager>(dispatcher,
                                                                    run_loop_thread,
                                                                    matching_dictionaries,
                                                                    std::chrono::milliseconds(1000));

  hid_manager->device_matched.connect([](auto&& registry_entry_id, auto&& device_ptr) {
    std::cout << "device_matched registry_entry_id:" << registry_entry_id << std::endl;

    if (device_ptr) {
      pqrs::osx::iokit_hid_device d(*device_ptr);

      if (auto manufacturer = d.find_manufacturer()) {
        std::cout << "  manufacturer: " << *manufacturer << std::endl;
      }

      if (auto product = d.find_product()) {
        std::cout << "  product: " << *product << std::endl;
      }
    }
  });

  hid_manager->device_terminated.connect([](auto&& registry_entry_id) {
    std::cout << "device_terminated registry_entry_id:" << registry_entry_id << std::endl;
  });

  hid_manager->error_occurred.connect([](auto&& message, auto&& kern_return) {
    std::cerr << "error_occurred " << message << " " << kern_return << std::endl;
  });

  hid_manager->async_start();

  // hid_manager->async_set_device_matched_delay(std::chrono::milliseconds(5000));

  // ============================================================

  global_wait->wait_notice();

  // ============================================================

  hid_manager = nullptr;

  run_loop_thread->terminate();
  run_loop_thread = nullptr;

  dispatcher->terminate();
  dispatcher = nullptr;

  std::cout << "finished" << std::endl;

  return 0;
}
