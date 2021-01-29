#include "telegram_client.hpp"
#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"


auto main(int argc, char **argv) -> int {
    CLI::App app("Sends random sticker to specified user");
    app.name("random_sticker_sender");

    std::string login;
    app.add_option("-l,--login", login, "Login of user to whom you want to send sticker")
            ->required();
    std::string encryption_key;
    app.add_option("-e,--encryption_key", encryption_key, "Key to encrypt session database")
            ->required();
    int amount = 1;
    app.add_option("-a,--amount", amount, "Amount of random stickers to send");

    CLI11_PARSE(app, argc, argv)
    TelegramClient client(login, encryption_key, amount);
    client.loop();
}
