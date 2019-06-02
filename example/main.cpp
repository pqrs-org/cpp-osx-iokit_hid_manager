#include <IOKit/hid/IOHIDUsageTables.h>
#include <csignal>
#include <pqrs/osx/iokit_hid_device.hpp>
#include <pqrs/osx/iokit_hid_manager.hpp>

namespace {
auto global_wait = pqrs::make_thread_wait();

class rescan_timer : public pqrs::dispatcher::extra::dispatcher_client {
public:
  rescan_timer(std::weak_ptr<pqrs::dispatcher::dispatcher> weak_dispatcher,
               std::shared_ptr<pqrs::osx::iokit_hid_manager> hid_manager) : dispatcher_client(weak_dispatcher),
                                                                            timer_(*this) {
    timer_.start(
        [hid_manager] {
          hid_manager->async_rescan();
        },
        std::chrono::milliseconds(2000));
  }

  ~rescan_timer(void) {
    detach_from_dispatcher([this] {
      timer_.stop();
    });
  }

private:
  pqrs::dispatcher::extra::timer timer_;
};
} // namespace

int main(void) {
  std::signal(SIGINT, [](int) {
    global_wait->notify();
  });

  auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
  auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);

  std::vector<pqrs::cf::cf_ptr<CFDictionaryRef>> matching_dictionaries{
      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::osx::iokit_hid_usage_page_generic_desktop,
          pqrs::osx::iokit_hid_usage_generic_desktop_keyboard),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::osx::iokit_hid_usage_page_generic_desktop,
          pqrs::osx::iokit_hid_usage_generic_desktop_mouse),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::osx::iokit_hid_usage_page_generic_desktop,
          pqrs::osx::iokit_hid_usage_generic_desktop_pointer),

      pqrs::osx::iokit_hid_manager::make_matching_dictionary(
          pqrs::osx::iokit_hid_usage_page_apple_vendor),
  };

  auto hid_manager = std::make_shared<pqrs::osx::iokit_hid_manager>(dispatcher,
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

  hid_manager->error_occurred.connect([](auto&& message, auto&& iokit_return) {
    std::cerr << "error_occurred " << message << " " << iokit_return << std::endl;
  });

  hid_manager->async_start();

  // hid_manager->async_set_device_matched_delay(std::chrono::milliseconds(5000));

  auto timer = std::make_unique<rescan_timer>(dispatcher,
                                              hid_manager);

  // ============================================================

  global_wait->wait_notice();

  // ============================================================

  timer = nullptr;
  hid_manager = nullptr;

  dispatcher->terminate();
  dispatcher = nullptr;

  std::cout << "finished" << std::endl;

  return 0;
}
