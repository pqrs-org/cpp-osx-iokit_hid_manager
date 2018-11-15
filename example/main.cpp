#include <IOKit/hid/IOHIDUsageTables.h>
#include <csignal>
#include <pqrs/cf_number.hpp>
#include <pqrs/osx/iokit_hid_manager.hpp>

namespace {
auto global_wait = pqrs::make_thread_wait();
}

int main(void) {
  pqrs::dispatcher::extra::initialize_shared_dispatcher();

  std::signal(SIGINT, [](int) {
    global_wait->notify();
  });

  std::vector<pqrs::cf_ptr<CFDictionaryRef>> matching_dictionaries;

  if (auto matching_dictionary = IOServiceMatching(kIOHIDDeviceKey)) {
    if (auto number = pqrs::make_cf_number(static_cast<int32_t>(kHIDPage_GenericDesktop))) {
      CFDictionarySetValue(matching_dictionary,
                           CFSTR(kIOHIDDeviceUsagePageKey),
                           *number);
    }
    if (auto number = pqrs::make_cf_number(static_cast<int32_t>(kHIDUsage_GD_Keyboard))) {
      CFDictionarySetValue(matching_dictionary,
                           CFSTR(kIOHIDDeviceUsageKey),
                           *number);
    }
    matching_dictionaries.emplace_back(matching_dictionary);

    CFRelease(matching_dictionary);
  }

  auto hid_manager = std::make_unique<pqrs::osx::iokit_hid_manager>(pqrs::dispatcher::extra::get_shared_dispatcher(),
                                                                    matching_dictionaries);

  hid_manager->device_detected.connect([](auto&& registry_entry_id, auto&& device_ptr) {
    std::cout << "device_detected registry_entry_id:" << registry_entry_id << std::endl;
  });

  hid_manager->device_removed.connect([](auto&& registry_entry_id) {
    std::cout << "device_remove registry_entry_id:" << registry_entry_id << std::endl;
  });

  hid_manager->error_occurred.connect([](auto&& message, auto&& iokit_return) {
    std::cerr << "error_occurred " << message << " " << iokit_return << std::endl;
  });

  hid_manager->async_start();

  // ============================================================

  global_wait->wait_notice();

  // ============================================================

  hid_manager = nullptr;

  pqrs::dispatcher::extra::terminate_shared_dispatcher();

  std::cout << "finished" << std::endl;

  return 0;
}
