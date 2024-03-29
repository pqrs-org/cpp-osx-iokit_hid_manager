#include <boost/ut.hpp>
#include <pqrs/osx/iokit_hid_manager.hpp>

int main(void) {
  using namespace boost::ut;
  using namespace boost::ut::literals;

  "iokit_hid_manager stress testing"_test = [] {
    auto time_source = std::make_shared<pqrs::dispatcher::hardware_time_source>();
    auto dispatcher = std::make_shared<pqrs::dispatcher::dispatcher>(time_source);
    auto run_loop_thread = std::make_shared<pqrs::cf::run_loop_thread>();

    auto object_id = pqrs::dispatcher::make_new_object_id();
    dispatcher->attach(object_id);

    for (int i = 0; i < 10000; ++i) {
      if (i % 100 == 0) {
        std::cout << "." << std::flush;
      }

      std::vector<pqrs::cf::cf_ptr<CFDictionaryRef>> matching_dictionaries;

      auto matching_dictionary = IOServiceMatching(kIOHIDDeviceKey);
      expect(matching_dictionary);
      matching_dictionaries.emplace_back(matching_dictionary);
      CFRelease(matching_dictionary);

      auto hid_manager = std::make_unique<pqrs::osx::iokit_hid_manager>(dispatcher,
                                                                        run_loop_thread,
                                                                        matching_dictionaries);

      hid_manager->device_matched.connect([](auto&& registry_entry_id, auto&& device_ptr) {
        // std::cout << "device_matched registry_entry_id:" << registry_entry_id << std::endl;
      });

      hid_manager->device_terminated.connect([](auto&& registry_entry_id) {
        // std::cout << "device_terminated registry_entry_id:" << registry_entry_id << std::endl;
      });

      hid_manager->async_start();

      // wait until iokit_hid_manager is started.
      {
        auto wait = pqrs::make_thread_wait();

        dispatcher->enqueue(
            object_id,
            [wait] {
              wait->notify();
            });

        wait->wait_notice();
      }

      hid_manager = nullptr;

      // wait until iokit_hid_manager is stopped.
      {
        auto wait = pqrs::make_thread_wait();

        dispatcher->enqueue(
            object_id,
            [wait] {
              wait->notify();
            });

        wait->wait_notice();
      }
    }

    std::cout << std::endl;

    dispatcher->detach(object_id);

    run_loop_thread->terminate();
    run_loop_thread = nullptr;

    dispatcher->terminate();
    dispatcher = nullptr;
  };

  return 0;
}
