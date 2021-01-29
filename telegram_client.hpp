#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <memory>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include <thread>
#include <random>

#include "utility.hpp"

std::mutex set_update_mutex;
const double WAIT_TIMEOUT = 10.0;
const time_t MESSAGE_SEND_WAIT_MS = 1000;

// Simple single-threaded example of TDLib usage.
// Real world programs should use separate thread for the user input.
// Example includes user authentication, receiving updates, getting chat list and sending text messages.

// overloaded
namespace detail {
    template<class... Fs>
    struct overload;

    template<class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };

    template<class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>, overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }

        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template<class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class TelegramClient {
public:
    TelegramClient(std::string login, std::string encryption_key, int amount) : login_(std::move(login)),
                                                                                encryption_key_(
                                                                                        std::move(encryption_key)),
                                                                                amount_(amount) {
        generator_.seed(std::chrono::system_clock::now().time_since_epoch().count());
        td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
        client_manager_ = std::make_unique<td::ClientManager>();
        client_id_ = client_manager_->create_client_id();
        send_query(td_api::make_object<td_api::getOption>("version"), {});
    }

    void loop() {
        while (true) {
            if (need_restart_) {
                restart();
            } else if (!are_authorized_) {
                process_response(client_manager_->receive(10));
            } else if (need_exit_) {
                return;
            } else if (!started_) {
                started_ = true;
                send_query(td_api::make_object<td_api::searchPublicChat>(login_),
                           [this](Object object) {
                               if (object->get_id() == td_api::error::ID) {
                                   return;
                               }
                               auto chat = td::move_tl_object_as<td_api::chat>(object);

                               selected_chat_ = chat->id_;
                           });
                send_query(td_api::make_object<td_api::getInstalledStickerSets>(),
                           [this](Object object) {
                               if (object->get_id() == td_api::error::ID) {
                                   return;
                               }
                               auto sticker_sets = td::move_tl_object_as<td_api::stickerSets>(object);
                               auto amount_of_sets = sticker_sets->sets_.size();
                               total_sets_ = amount_of_sets;

                               for (auto i = 0; i < amount_of_sets; i++) {
                                   send_query(td_api::make_object<td_api::getStickerSet>(sticker_sets->sets_[i]->id_),
                                              [this](Object object) {
                                                  if (object->get_id() == td_api::error::ID) {
                                                      return;
                                                  }
                                                  std::lock_guard<std::mutex> guard(set_update_mutex);
                                                  auto stickers = td::move_tl_object_as<td_api::stickerSet>(object);
                                                  processed_sets_++;
                                                  std::move(std::begin(stickers->stickers_),
                                                            std::end(stickers->stickers_),
                                                            std::back_inserter(stickers_));
                                              });
                               }
                           });
            } else if (total_sets_ == processed_sets_ && selected_chat_ != -1 && sent_ < amount_) {
                sent_++;
                auto send_message = td_api::make_object<td_api::sendMessage>();
                send_message->chat_id_ = selected_chat_;
                auto message_content = td_api::make_object<td_api::inputMessageSticker>();
                auto sticker_file = td_api::make_object<td_api::inputFileRemote>();

                auto sticker_number = get_random_sticker_number();

                sticker_file->id_ = stickers_[sticker_number]->sticker_->remote_->id_;
                message_content->sticker_ = std::move(sticker_file);
                message_content->height_ = stickers_[sticker_number]->height_;
                message_content->width_ = stickers_[sticker_number]->width_;
                send_message->input_message_content_ = std::move(message_content);

                send_query(std::move(send_message), [this](Object object) {
                    if (object->get_id() == td_api::error::ID) {
                        return;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(MESSAGE_SEND_WAIT_MS));
                    std::lock_guard<std::mutex> guard(set_update_mutex);
                    delivered_++;
                    if (delivered_ == amount_) {
                        need_exit_ = true;
                    }
                });
            } else {
                while (true) {
                    auto response = client_manager_->receive(WAIT_TIMEOUT);
                    if (response.object == nullptr) {
                        continue;
                    }

                    process_response(std::move(response));
                    break;
                }
            }
        }
    }

private:
    std::string login_;
    std::string encryption_key_;
    int amount_;

    using Object = td_api::object_ptr<td_api::Object>;
    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t client_id_{0};
    std::default_random_engine generator_;
    td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
    bool are_authorized_{false};
    bool need_restart_{false};
    bool need_exit_{false};
    bool started_{false};
    int sent_ = 0;
    int delivered_ = 0;
    long total_sets_{-1};
    long processed_sets_{0};
    td_api::int32 selected_chat_{-1};
    std::vector<td::tl::unique_ptr<td_api::sticker>> stickers_{};
    std::uint64_t current_query_id_{0};
    std::uint64_t authentication_query_id_{0};

    std::map<std::uint64_t, std::function<void(Object)>> handlers_;

    std::map<std::int32_t, td_api::object_ptr<td_api::user>> users_;

    std::map<std::int64_t, std::string> chat_title_;

    void restart() {
        client_manager_.reset();
        *this = TelegramClient(login_, encryption_key_, amount_);
    }

    size_t get_random_sticker_number() {
        std::uniform_int_distribution<int> distribution(0, stickers_.size());
        return distribution(generator_);
    }

    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
        auto query_id = next_query_id();
        if (handler) {
            handlers_.emplace(query_id, std::move(handler));
        }
        client_manager_->send(client_id_, query_id, std::move(f));
    }

    void process_response(td::ClientManager::Response response) {
        if (!response.object) {
            return;
        }
        //std::cout << response.request_id << " " << to_string(response.object) << std::endl;
        if (response.request_id == 0) {
            return process_update(std::move(response.object));
        }
        auto it = handlers_.find(response.request_id);
        if (it != handlers_.end()) {
            it->second(std::move(response.object));
        }
    }

    std::string get_user_name(std::int32_t user_id) const {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return "unknown user";
        }
        return it->second->first_name_ + " " + it->second->last_name_;
    }

    std::string get_chat_title(std::int64_t chat_id) const {
        auto it = chat_title_.find(chat_id);
        if (it == chat_title_.end()) {
            return "unknown chat";
        }
        return it->second;
    }

    void process_update(td_api::object_ptr<td_api::Object> update) {
        td_api::downcast_call(
                *update, overloaded(
                        [this](td_api::updateAuthorizationState &update_authorization_state) {
                            authorization_state_ = std::move(update_authorization_state.authorization_state_);
                            on_authorization_state_update();
                        },
                        [](auto &update) {}));
    }

    auto create_authentication_query_handler() {
        return [this, id = authentication_query_id_](Object object) {
            if (id == authentication_query_id_) {
                check_authentication_error(std::move(object));
            }
        };
    }

    void on_authorization_state_update() {
        authentication_query_id_++;
        td_api::downcast_call(
                *authorization_state_,
                overloaded(
                        [this](td_api::authorizationStateReady &) {
                            are_authorized_ = true;
                        },
                        [this](td_api::authorizationStateLoggingOut &) {
                            are_authorized_ = false;
                        },
                        [this](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
                        [this](td_api::authorizationStateClosed &) {
                            are_authorized_ = false;
                            need_restart_ = true;
                        },
                        [this](td_api::authorizationStateWaitCode &) {
                            std::cout << "Enter authentication code: " << std::flush;
                            std::string code;
                            std::cin >> code;
                            send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitPhoneNumber &) {
                            std::cout << "Enter phone number: " << std::flush;
                            std::string phone_number;
                            std::cin >> phone_number;
                            send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitEncryptionKey &) {
                            send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(encryption_key_),
                                       create_authentication_query_handler());
                        },
                        [this](td_api::authorizationStateWaitTdlibParameters &) {
                            auto parameters = td_api::make_object<td_api::tdlibParameters>();
                            parameters->database_directory_ = *utility::join_path(utility::get_home(), std::make_unique<std::string>(".tdlib"));
                            parameters->use_message_database_ = true;
                            parameters->use_secret_chats_ = true;
                            parameters->api_id_ = 2109835;
                            parameters->api_hash_ = "3cd5aae58fe1f3803f08f6a954602a22";
                            parameters->system_language_code_ = "en";
                            parameters->device_model_ = "Desktop";
                            parameters->application_version_ = "1.0";
                            parameters->enable_storage_optimizer_ = true;
                            send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                                       create_authentication_query_handler());
                        },
                        [](auto &update) {}));
    }

    void check_authentication_error(Object object) {
        if (object->get_id() == td_api::error::ID) {
            auto error = td::move_tl_object_as<td_api::error>(object);
            std::cout << "Error: " << to_string(error) << std::flush;
            on_authorization_state_update();
        }
    }

    std::uint64_t next_query_id() {
        return ++current_query_id_;
    }
};