#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtkmm.h>
#include "pay.h"

using namespace Gtk;
using namespace Glib;

class MainWindow : public Gtk::Window {
public:
    MainWindow() {
        auto cssProvider = Gtk::CssProvider::create();
        cssProvider->load_from_data(R"(
            .error { border: 1px solid red; color: red; }
            .success { border: 1px solid green; color: green; }
            .warning { border: 1px solid orange; color: orange; }
            .connection_fail { border: 1px solid red; color: red; }
            .connection_success { border: 1px solid green; color: green; }
            .negative_balance { color: #c00000; }
            button.wide-button { min-width: 50px; }
        )");
        auto screen = Gdk::Screen::get_default();
        auto styleContext = get_style_context();
        styleContext->add_provider_for_screen(screen, cssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        set_title("Not Yet Secure P2P Micropayment System");
        set_default_size(600, 500);

        mainBox.set_orientation(Orientation::ORIENTATION_VERTICAL);
        add(mainBox);

        mainBox.pack_start(userStatusGrid, false, false);
        mainBox.pack_start(scrolledWindow, true, true);

        // top part
        userStatusGrid.set_row_spacing(10);
        userStatusGrid.set_column_spacing(10);
        userStatusGrid.set_margin_top(15);
        userStatusGrid.set_margin_bottom(15);
        userStatusGrid.set_margin_start(15);
        userStatusGrid.set_margin_end(15);

        Pango::FontDescription bigFont;
        bigFont.set_size(14 * PANGO_SCALE);
        bigFont.set_weight(Pango::Weight::WEIGHT_BOLD);
        bigFont.set_family("monospace");

        userStatusGrid.attach(usernameHeader, 0, 0, 1, 1);
        usernameHeader.set_text("My Username ");
        usernameHeader.set_halign(Align::ALIGN_START);

        userStatusGrid.attach(usernameLabel, 0, 1, 1, 1);
        usernameLabel.set_text(clientAction.username);
        usernameLabel.override_font(bigFont);
        usernameLabel.set_halign(Align::ALIGN_START);
        // usernameLabel.set_size_request(150, -1);

        userStatusGrid.attach(accountBalanceHeader, 1, 0, 1, 1);
        accountBalanceHeader.set_text("Balance    ");
        accountBalanceHeader.set_halign(Align::ALIGN_START);

        userStatusGrid.attach(accountBalanceLabel, 1, 1, 1, 1);
        accountBalanceLabel.set_text(std::to_string(clientAction.accountBalance));
        accountBalanceLabel.override_font(bigFont);
        accountBalanceLabel.set_halign(Align::ALIGN_START);
        // accountBalanceLabel.set_size_request(150, -1);

        userStatusGrid.attach(serverAddressHeader, 3, 0, 1, 1);
        serverAddressHeader.set_text("Central Server    ");
        serverAddressHeader.set_halign(Align::ALIGN_START);

        userStatusGrid.attach(serverAddressLabel, 3, 1, 1, 1);
        serverAddressLabel.set_text(clientAction.serverAddress + ":" + clientAction.port);
        serverAddressLabel.override_font(bigFont);
        serverAddressLabel.set_halign(Align::ALIGN_START);

        serverAddressLabel.set_hexpand(true);

        userStatusGrid.attach(refreshButton, 4, 0, 1, 2);
        refreshButton.set_label("Refresh");
        refreshButton.signal_clicked().connect([this]() {
            clientAction.fetchServerInfo();
        });

        userStatusGrid.attach(logOutButton, 5, 0, 1, 2);
        logOutButton.set_label("Log out");
        logOutButton.signal_clicked().connect([this]() {
            clientAction.loggedIn = false;
            hide();
        });

        userStatusGrid.attach(usernameFilterEntry, 0, 2, 2, 1);
        usernameFilterEntry.set_placeholder_text("Filter by username");
        usernameFilterEntry.signal_changed().connect([this]() {
            updateOnlineUsers();
        });

        // bottom part
        scrolledWindow.add(onlineUsersGrid);

        onlineUsersGrid.set_row_spacing(5);
        onlineUsersGrid.set_column_spacing(10);
        onlineUsersGrid.set_margin_top(15);
        onlineUsersGrid.set_margin_bottom(15);
        onlineUsersGrid.set_margin_start(15);
        onlineUsersGrid.set_margin_end(15);

        updateOnlineUsers();

        show_all_children();
    }

    void on_show() override {
        Window::on_show();
        updateUserStatus();
        updateOnlineUsers();

        clientAction.statusUpdatedCallback = [this]() {
            updateUserStatus();
            updateOnlineUsers();
            show_all_children();
        };

        Glib::signal_timeout().connect(sigc::mem_fun(*this, &MainWindow::autoRefresh), 1000);
    }

    bool autoRefresh() {
        if (!clientAction.loggedIn)
            return false; // stop the loop until re-logged in and on_show is called again
        clientAction.fetchServerInfo();
        return true;
    }

    void updateUserStatus() {
        usernameLabel.set_text(clientAction.username);
        accountBalanceLabel.set_text(std::to_string(clientAction.accountBalance));
        serverAddressLabel.set_text(clientAction.serverAddress + ":" + clientAction.port);

        if (clientAction.accountBalance < 0) {
            accountBalanceLabel.get_style_context()->add_class("negative_balance");
        } else {
            accountBalanceLabel.get_style_context()->remove_class("negative_balance");
        }
    }

    void updateOnlineUsers() {
        for (auto &child : onlineUsersGrid.get_children()) {
            onlineUsersGrid.remove(*child);
        }

        auto filteredUsers = clientAction.userAccounts;
        std::string filter = usernameFilterEntry.get_text();
        if (!filter.empty()) {
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            filteredUsers.erase(std::remove_if(filteredUsers.begin(), filteredUsers.end(), [filter](const UserAccount &user) {
                                    std::string username = user.username;
                                    std::transform(username.begin(), username.end(), username.begin(), ::tolower);
                                    return username.find(filter) == std::string::npos;
                                }),
                                filteredUsers.end());
        }

        Pango::FontDescription tableHeaderFont;
        tableHeaderFont.set_style(Pango::Style::STYLE_ITALIC);

        Label *onlineUsernameHeader = manage(new Label("Username     "));
        onlineUsersGrid.attach(*onlineUsernameHeader, 0, 0, 1, 1);
        onlineUsernameHeader->override_font(tableHeaderFont);

        Label *onlineIPHeader = manage(new Label("IP Address     "));
        onlineUsersGrid.attach(*onlineIPHeader, 1, 0, 1, 1);
        onlineIPHeader->override_font(tableHeaderFont);

        Label *onlinePortHeader = manage(new Label("Port     "));
        onlineUsersGrid.attach(*onlinePortHeader, 2, 0, 1, 1);
        onlinePortHeader->override_font(tableHeaderFont);
        onlinePortHeader->set_halign(Align::ALIGN_START);
        onlinePortHeader->set_hexpand(true);

        Label *onlinePayButtonHeader = manage(new Label());
        onlineUsersGrid.attach(*onlinePayButtonHeader, 3, 0, 1, 1);
        onlinePayButtonHeader->override_font(tableHeaderFont);

        Label *onlineChatButtonHeader = manage(new Label());
        onlineUsersGrid.attach(*onlineChatButtonHeader, 4, 0, 1, 1);
        onlineChatButtonHeader->override_font(tableHeaderFont);

        Pango::FontDescription tableFont;
        tableFont.set_family("monospace");
        tableFont.set_size(12 * PANGO_SCALE);

        for (auto &user : filteredUsers) {
            Label *usernameLabel = manage(new Label(user.username));
            usernameLabel->get_style_context()->add_class("gridlines");
            usernameLabel->set_halign(Align::ALIGN_START);
            usernameLabel->override_font(tableFont);
            onlineUsersGrid.attach(*usernameLabel, 0, onlineUsersGrid.get_children().size() / 5, 1, 1);

            Label *ipLabel = manage(new Label(user.ipAddr));
            ipLabel->get_style_context()->add_class("gridlines");
            ipLabel->set_halign(Align::ALIGN_START);
            ipLabel->override_font(tableFont);
            onlineUsersGrid.attach(*ipLabel, 1, onlineUsersGrid.get_children().size() / 5, 1, 1);

            Label *portLabel = manage(new Label(user.p2pPort));
            portLabel->get_style_context()->add_class("gridlines");
            portLabel->set_halign(Align::ALIGN_START);
            portLabel->override_font(tableFont);
            onlineUsersGrid.attach(*portLabel, 2, onlineUsersGrid.get_children().size() / 5, 1, 1);

            Button *payButton = manage(new Button("Pay"));
            if (user.username == clientAction.username)
                payButton->set_sensitive(false);
            else
                payButton->get_style_context()->add_class("suggested-action");
            payButton->get_style_context()->add_class("wide-button");
            onlineUsersGrid.attach(*payButton, 3, onlineUsersGrid.get_children().size() / 5, 1, 1);

            payButton->signal_clicked().connect([this, user]() {
                payWindow.payeeUsername = user.username;
                payWindow.set_transient_for(*this);
                payWindow.show_all();
            });

            Button *detailsButton = manage(new Button("Details"));
            detailsButton->get_style_context()->add_class("wide-button");
            onlineUsersGrid.attach(*detailsButton, 4, onlineUsersGrid.get_children().size() / 5, 1, 1);
        }

        if (filteredUsers.size() != clientAction.userAccounts.size()) {
            Label *filteredLabel = manage(new Label("Some users are hidden by the filter"));
            filteredLabel->override_font(tableHeaderFont);
            filteredLabel->set_halign(Align::ALIGN_CENTER);
            onlineUsersGrid.attach(*filteredLabel, 0, onlineUsersGrid.get_children().size() / 5, 5, 1);
        }

        show_all_children();
    }

private:
    Box mainBox;

    Label signInTitle;
    Button configureServerButton;

    // top
    Grid userStatusGrid;
    Label usernameHeader;
    Label usernameLabel;
    Label accountBalanceHeader;
    Label accountBalanceLabel;
    Label serverAddressHeader;
    Label serverAddressLabel;
    Button refreshButton;
    Button logOutButton;

    // bottom
    ScrolledWindow scrolledWindow;
    Grid onlineUsersGrid;
    Entry usernameFilterEntry;

    PayWindow payWindow;
};

#endif // MAIN_WINDOW_H