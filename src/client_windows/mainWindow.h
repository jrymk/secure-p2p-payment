#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtkmm.h>
#include "payWindow.h"

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
            .deep_red { color: #c00000; }
            .fetch_fail { color: #909090; }
            button.wide-button { min-width: 50px; }
        )");
        auto screen = Gdk::Screen::get_default();
        auto styleContext = get_style_context();
        styleContext->add_provider_for_screen(screen, cssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        set_title("Not Yet Secure P2P Micropayment System");
        set_default_size(520, 400);

        mainBox.set_orientation(Orientation::ORIENTATION_VERTICAL);
        add(mainBox);

        mainBox.pack_start(userStatusGrid, false, false);
        mainBox.pack_start(scrolledWindow, true, true);

        // top part
        userStatusGrid.set_row_spacing(5);
        userStatusGrid.set_column_spacing(10);
        userStatusGrid.set_margin_top(15);
        userStatusGrid.set_margin_bottom(15);
        userStatusGrid.set_margin_start(15);
        userStatusGrid.set_margin_end(15);

        Pango::FontDescription bigFont;
        bigFont.set_size(18 * PANGO_SCALE);
        bigFont.set_weight(Pango::Weight::WEIGHT_BOLD);
        bigFont.set_family("monospace");

        Grid *topLeftGrid = manage(new Grid());
        userStatusGrid.attach(*topLeftGrid, 0, 0, 2, 3);

        topLeftGrid->set_row_spacing(5);
        topLeftGrid->set_column_spacing(10);

        topLeftGrid->attach(usernameHeader, 0, 0, 1, 1);
        usernameHeader.set_text("My Username  ");
        usernameHeader.set_halign(Align::ALIGN_START);

        topLeftGrid->attach(usernameLabel, 0, 1, 1, 1);
        usernameLabel.override_font(bigFont);
        usernameLabel.set_halign(Align::ALIGN_START);
        // usernameLabel.set_size_request(150, -1);

        topLeftGrid->attach(accountBalanceHeader, 1, 0, 1, 1);
        accountBalanceHeader.set_text("Balance    ");
        accountBalanceHeader.set_halign(Align::ALIGN_START);
        accountBalanceHeader.set_hexpand(true);

        topLeftGrid->attach(accountBalanceLabel, 1, 1, 1, 1);
        accountBalanceLabel.override_font(bigFont);
        accountBalanceLabel.set_halign(Align::ALIGN_START);

        Pango::FontDescription mono;
        mono.set_family("monospace");

        userStatusGrid.attach(serverAddressLabel, 2, 0, 3, 1);
        serverAddressLabel.set_text("No data");
        serverAddressLabel.set_halign(Align::ALIGN_END);
        serverAddressLabel.override_font(mono);

        userStatusGrid.attach(serverDetailsButton, 3, 1, 1, 1);
        serverDetailsButton.set_label("Details");
        serverDetailsButton.set_sensitive(false);
        serverDetailsButton.signal_clicked().connect([this]() {
            // clientAction.fetchServerInfo();
        });

        userStatusGrid.attach(logOutButton, 4, 1, 1, 1);
        logOutButton.set_label("Log out");
        logOutButton.signal_clicked().connect([this]() {
            logOut();
        });

        filterGrid.set_column_spacing(10);
        onlineUsersGrid.attach(filterGrid, 3, 0, 3, 1);
        Label *filterLabel = manage(new Label("Filter:"));
        filterGrid.attach(*filterLabel, 0, 0, 1, 1);
        filterLabel->set_halign(Align::ALIGN_END);
        filterGrid.set_halign(Align::ALIGN_END);

        filterGrid.attach(usernameFilterEntry, 1, 0, 1, 1);
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

        fetchOk = true;

        updateAll();

        clientAction.statusUpdatedCallback = [this]() {
            updateAll();
        };
        clientAction.sessionEndedCallback = [this]() {
            fetchOk = false;
            updateAll();

            MessageDialog dialog(*this, "Session ended", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text("Server has ended your session. Please log in again");
            dialog.run();
            logOut();
        };

        signal_timeout().connect(sigc::mem_fun(*this, &MainWindow::autoRefresh), 1000);
    }

    bool autoRefresh() {
        if (!clientAction.loggedIn)
            return false; // stop the loop until re-logged in and on_show is called again
        fetchOk = clientAction.fetchServerInfo();
        return true;
    }

    void updateAll() {
        updateUserStatus();
        updateOnlineUsers();
        show_all_children();
    }

    void updateUserStatus() {
        usernameLabel.set_text(clientAction.username);
        accountBalanceLabel.set_text(std::to_string(clientAction.accountBalance));
        if (fetchOk) {
            serverAddressLabel.set_text("Connected to " + clientAction.serverAddress + ":" + clientAction.port);
            serverAddressLabel.get_style_context()->remove_class("deep_red");
        } else {
            serverAddressLabel.set_text("Reconnecting to " + clientAction.serverAddress + ":" + clientAction.port);
            serverAddressLabel.get_style_context()->add_class("deep_red");
        }

        if (clientAction.accountBalance < 0) {
            accountBalanceLabel.get_style_context()->add_class("deep_red");
        } else {
            accountBalanceLabel.get_style_context()->remove_class("deep_red");
        }
    }

    void updateOnlineUsers() {
        for (auto &child : onlineUsersGrid.get_children()) {
            if (child != &filterGrid)
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

        Label *onlineUsernameHeader = manage(new Label("Username  "));
        onlineUsersGrid.attach(*onlineUsernameHeader, 0, 0, 1, 1);
        onlineUsernameHeader->override_font(tableHeaderFont);
        onlineUsernameHeader->set_halign(Align::ALIGN_START);

        Label *onlineIPHeader = manage(new Label("Transfer Address  "));
        onlineUsersGrid.attach(*onlineIPHeader, 1, 0, 1, 1);
        onlineIPHeader->override_font(tableHeaderFont);
        onlineIPHeader->set_halign(Align::ALIGN_START);

        onlineIPHeader->set_hexpand(true);

        Pango::FontDescription tableFont;
        tableFont.set_family("monospace");
        tableFont.set_size(12 * PANGO_SCALE);

        for (auto &user : filteredUsers) {
            Label *usernameLabel = manage(new Label(user.username));
            usernameLabel->set_halign(Align::ALIGN_START);
            usernameLabel->override_font(tableFont);
            if (!fetchOk)
                usernameLabel->get_style_context()->add_class("fetch_fail");
            onlineUsersGrid.attach(*usernameLabel, 0, (onlineUsersGrid.get_children().size() + 1) / 4, 1, 1);

            Label *ipLabel = manage(new Label(user.ipAddr + ":" + user.p2pPort));
            ipLabel->set_halign(Align::ALIGN_START);
            ipLabel->override_font(tableFont);
            if (!fetchOk)
                ipLabel->get_style_context()->add_class("fetch_fail");
            onlineUsersGrid.attach(*ipLabel, 1, (onlineUsersGrid.get_children().size() + 1) / 4, 1, 1);

            Button *payButton = manage(new Button("Pay"));
            if (user.username == clientAction.username)
                payButton->set_sensitive(false);
            else
                payButton->get_style_context()->add_class("suggested-action");
            payButton->get_style_context()->add_class("wide-button");
            onlineUsersGrid.attach(*payButton, 4, (onlineUsersGrid.get_children().size() + 1) / 4, 1, 1);

            payButton->signal_clicked().connect([this, user]() {
                payWindow.payeeUsername = user.username;
                payWindow.set_transient_for(*this);
                payWindow.show_all();
            });

            Button *detailsButton = manage(new Button("Details"));
            detailsButton->get_style_context()->add_class("wide-button");
            detailsButton->set_sensitive(false);
            onlineUsersGrid.attach(*detailsButton, 5, (onlineUsersGrid.get_children().size() + 1) / 4, 1, 1);
        }

        if (filteredUsers.size() != clientAction.userAccounts.size()) {
            Label *filteredLabel = manage(new Label("Some users are hidden by the filter"));
            filteredLabel->override_font(tableHeaderFont);
            filteredLabel->set_halign(Align::ALIGN_CENTER);
            onlineUsersGrid.attach(*filteredLabel, 0, (onlineUsersGrid.get_children().size() + 1) / 4, 6, 1);
        }

        show_all_children();
    }

    void logOut() {
        clientAction.logOut();
        fetchOk = false;
        hide();
    }

private:
    bool fetchOk = false;

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
    Button serverDetailsButton;
    Button logOutButton;

    Grid filterGrid;

    // bottom
    ScrolledWindow scrolledWindow;
    Grid onlineUsersGrid;
    Entry usernameFilterEntry;

    PayWindow payWindow;
};

#endif // MAIN_WINDOW_H