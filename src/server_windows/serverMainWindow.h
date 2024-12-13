#ifndef SERVER_MAIN_WINDOW_H
#define SERVER_MAIN_WINDOW_H

#include <gtkmm.h>
#include <vector>
#include "../serverAction.h"

using namespace Gtk;
using namespace Glib;

class ServerMainWindow : public Gtk::Window {
public:
    ServerMainWindow() {
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
        set_default_size(650, 400);

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

        topLeftGrid->attach(onlineUsersHeader, 0, 0, 1, 1);
        onlineUsersHeader.set_text("Online Users  ");
        onlineUsersHeader.set_halign(Align::ALIGN_START);

        topLeftGrid->attach(onlineUsersLabel, 0, 1, 1, 1);
        onlineUsersLabel.override_font(bigFont);
        onlineUsersLabel.set_halign(Align::ALIGN_START);
        // onlineUsersLabel.set_size_request(150, -1);

        topLeftGrid->attach(totalUsersHeader, 1, 0, 1, 1);
        totalUsersHeader.set_text("Total Users  ");
        totalUsersHeader.set_halign(Align::ALIGN_START);
        totalUsersHeader.set_hexpand(true);

        topLeftGrid->attach(totalUsersLabel, 1, 1, 1, 1);
        totalUsersLabel.override_font(bigFont);
        totalUsersLabel.set_halign(Align::ALIGN_START);

        Pango::FontDescription mono;
        mono.set_family("monospace");

        userStatusGrid.attach(serverAddressLabel, 2, 0, 3, 1);
        serverAddressLabel.set_text("Connected");
        serverAddressLabel.set_halign(Align::ALIGN_END);
        serverAddressLabel.override_font(mono);

        Label *dummy = manage(new Label());
        userStatusGrid.attach(*dummy, 2, 1, 1, 1);
        dummy->set_hexpand(true);

        userStatusGrid.attach(serverDetailsButton, 3, 1, 1, 1);
        serverDetailsButton.set_label("Details");
        serverDetailsButton.set_sensitive(false);
        serverDetailsButton.signal_clicked().connect([this]() {
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
            updateUsers();
        });

        // bottom part
        scrolledWindow.add(onlineUsersGrid);

        onlineUsersGrid.set_row_spacing(5);
        onlineUsersGrid.set_column_spacing(10);
        onlineUsersGrid.set_margin_top(15);
        onlineUsersGrid.set_margin_bottom(15);
        onlineUsersGrid.set_margin_start(15);
        onlineUsersGrid.set_margin_end(15);

        updateUsers();

        show_all_children();
    }

    void on_show() override {
        Window::on_show();

        fetchOk = true;

        updateAll();

        signal_timeout().connect(sigc::mem_fun(*this, &ServerMainWindow::autoRefresh), 1000);
    }

    bool autoRefresh() {
        updateAll();
        return true;
    }

    void updateAll() {
        updateUsers();
        show_all_children();
    }

    void updateUsers() {
        for (auto &child : onlineUsersGrid.get_children()) {
            if (child != &filterGrid)
                onlineUsersGrid.remove(*child);
        }

        std::vector<std::pair<std::string, std::pair<OnlineEntry *, int>>> allValidUsers;

        for (auto &user : serverAction.onlineUsers) {
            if (user.username.empty())
                continue;
            if (user.username.find(usernameFilterEntry.get_text()) == std::string::npos)
                continue;
            allValidUsers.push_back({user.username, {&user, 0}});
        }

        size_t cnt = allValidUsers.size();

        for (auto &user : serverAction.userAccounts) {
            bool found = false;
            for (size_t i = 0; i < cnt; i++) {
                if (allValidUsers[i].first == user.username) {
                    allValidUsers[i].second.second = user.balance;
                    found = true;
                    break;
                }
            }
            if (!found) {
                allValidUsers.push_back({user.username, {nullptr, user.balance}});
            }
        }

        size_t prefilteredSize = allValidUsers.size();

        std::vector<std::pair<std::string, std::pair<OnlineEntry *, int>>> filteredUsers = allValidUsers;

        std::string filter = usernameFilterEntry.get_text();
        // if (!filter.empty()) {
        //     std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        //     filteredUsers.erase(std::remove_if(filteredUsers.begin(), filteredUsers.end(), [filter](const UserAccount &user) {
        //                             std::string username = user.username;
        //                             std::transform(username.begin(), username.end(), username.begin(), ::tolower);
        //                             return username.find(filter) == std::string::npos;
        //                         }),
        //                         filteredUsers.end());
        // }

        Pango::FontDescription tableHeaderFont;
        tableHeaderFont.set_style(Pango::Style::STYLE_ITALIC);

        Label *onlineUsersHeader = manage(new Label("Username  "));
        onlineUsersGrid.attach(*onlineUsersHeader, 0, 0, 1, 1);
        onlineUsersHeader->override_font(tableHeaderFont);
        onlineUsersHeader->set_halign(Align::ALIGN_START);

        Label *onlineIPHeader = manage(new Label("Transfer Address  "));
        onlineUsersGrid.attach(*onlineIPHeader, 1, 0, 1, 1);
        onlineIPHeader->override_font(tableHeaderFont);
        onlineIPHeader->set_halign(Align::ALIGN_START);

        Label *balanceHeader = manage(new Label("Balance  "));
        onlineUsersGrid.attach(*balanceHeader, 2, 0, 1, 1);
        balanceHeader->override_font(tableHeaderFont);
        balanceHeader->set_halign(Align::ALIGN_START);

        onlineIPHeader->set_hexpand(true);

        Pango::FontDescription tableFont;
        tableFont.set_family("monospace");
        tableFont.set_size(12 * PANGO_SCALE);

        for (auto &user : filteredUsers) {
            Label *onlineUsersLabel = manage(new Label(user.first));
            onlineUsersLabel->set_halign(Align::ALIGN_START);
            onlineUsersLabel->override_font(tableFont);
            if (!fetchOk)
                onlineUsersLabel->get_style_context()->add_class("fetch_fail");
            onlineUsersGrid.attach(*onlineUsersLabel, 0, (onlineUsersGrid.get_children().size() + 1) / 5, 1, 1);

            std::string address = "N/A";

            if (user.second.first != nullptr)
                address = user.second.first->ipAddr + ":" + std::to_string(user.second.first->clientPort);

            Label *ipLabel = manage(new Label(address));
            ipLabel->set_halign(Align::ALIGN_START);
            ipLabel->override_font(tableFont);
            if (!fetchOk)
                ipLabel->get_style_context()->add_class("fetch_fail");
            onlineUsersGrid.attach(*ipLabel, 1, (onlineUsersGrid.get_children().size() + 1) / 5, 1, 1);

            Label *balanceLabel = manage(new Label(std::to_string(user.second.second)));
            balanceLabel->set_halign(Align::ALIGN_START);
            balanceLabel->override_font(tableFont);
            if (!fetchOk)
                balanceLabel->get_style_context()->add_class("fetch_fail");
            onlineUsersGrid.attach(*balanceLabel, 2, (onlineUsersGrid.get_children().size() + 1) / 5, 1, 1);

            Button *onlineButton = manage(new Button(user.second.first == nullptr ? "Offline" : "Online"));
            if (user.second.first == nullptr)
                onlineButton->set_sensitive(false);
            else
                onlineButton->get_style_context()->add_class("success");
            onlineButton->get_style_context()->add_class("wide-button");
            onlineUsersGrid.attach(*onlineButton, 4, (onlineUsersGrid.get_children().size() + 1) / 5, 1, 1);

            std::string secondary = "User is offline";
            if (user.second.first != nullptr)
                secondary = "IP: " + user.second.first->ipAddr + "\nPort: " + std::to_string(user.second.first->clientPort);

            onlineButton->signal_clicked().connect([this, secondary]() {
                MessageDialog dialog(*this, "User details", false, MessageType::MESSAGE_INFO, ButtonsType::BUTTONS_OK, true);
                dialog.set_secondary_text(secondary);
                dialog.run();
            });

            Button *detailsButton = manage(new Button("Details"));
            detailsButton->get_style_context()->add_class("wide-button");
            onlineUsersGrid.attach(*detailsButton, 5, (onlineUsersGrid.get_children().size() + 1) / 5, 1, 1);

            detailsButton->signal_clicked().connect([this, user]() {
                MessageDialog dialog(*this, "User details", false, MessageType::MESSAGE_INFO, ButtonsType::BUTTONS_OK, true);
                dialog.set_secondary_text("IP: " + user.second.first->ipAddr + "\nPort: " + std::to_string(user.second.first->clientPort) + "\nPublic key: \n" + user.second.first->publicKey);
                dialog.run();
            });
        }

        if (filteredUsers.size() != prefilteredSize) {
            Label *filteredLabel = manage(new Label("Some users are hidden by the filter"));
            filteredLabel->override_font(tableHeaderFont);
            filteredLabel->set_halign(Align::ALIGN_CENTER);
            onlineUsersGrid.attach(*filteredLabel, 0, (onlineUsersGrid.get_children().size() + 1) / 5, 6, 1);
        }

        // std::vector<OnlineEntry> filteredUsers;
        // for (const auto &user : serverAction.onlineUsers) {
        //     if (user.username.find(usernameFilterEntry.get_text()) != std::string::npos)
        //         filteredUsers.push_back(user);
        // }

        onlineUsersLabel.set_text(std::to_string(cnt));
        totalUsersLabel.set_text(std::to_string(serverAction.userAccounts.size()));

        show_all_children();
    }

private:
    bool fetchOk = false;

    Box mainBox;

    Label signInTitle;
    Button configureServerButton;

    // top
    Grid userStatusGrid;
    Label onlineUsersHeader;
    Label onlineUsersLabel;
    Label totalUsersHeader;
    Label totalUsersLabel;
    Label serverAddressHeader;
    Label serverAddressLabel;
    Button serverDetailsButton;

    Grid filterGrid;

    // bottom
    ScrolledWindow scrolledWindow;
    Grid onlineUsersGrid;
    Entry usernameFilterEntry;
};

#endif // SERVER_MAIN_WINDOW_H