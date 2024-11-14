#ifndef LOGIN_H
#define LOGIN_H

#include <gtkmm.h>
#include <iostream>
#include <string>
#include <cctype>
#include "configureServerWindow.h"
#include "registerWindow.h"
#include <thread>
#include <atomic>

using namespace Glib;
using namespace Gtk;

class LoginWindow : public Window {
public:
    std::string username;
    std::string clientPort;

    LoginWindow() {
        set_title("Secure P2P Micropayment System");
        set_default_size(200, 200);

        add(grid);

        grid.set_row_spacing(10);
        grid.set_column_spacing(10);
        grid.set_margin_top(15);
        grid.set_margin_bottom(15);
        grid.set_margin_start(15);
        grid.set_margin_end(15);

        grid.attach(top, 0, 0, 4, 1);
        top.set_column_spacing(10);

        signInTitle.set_text("Sign in");
        Pango::FontDescription headingFont;
        headingFont.set_size(20 * PANGO_SCALE);
        headingFont.set_weight(Pango::Weight::WEIGHT_BOLD);
        signInTitle.override_font(headingFont);
        signInTitle.set_halign(Align::ALIGN_START);
        signInTitle.set_hexpand(true);
        top.attach(signInTitle, 0, 0, 1, 1);

        configureServerButton.set_label("Configure server");
        configureServerButton.signal_clicked().connect([this]() {
            configureServerButton.get_style_context()->remove_class("error");
            configureServerWindow.set_transient_for(*this);
            configureServerWindow.show_all();
        });
        top.attach(configureServerButton, 1, 0, 1, 1);

        usernameHeader.set_text("Username:");
        usernameHeader.set_halign(Align::ALIGN_END);
        grid.attach(usernameHeader, 0, 1, 1, 1);

        usernameEntry.set_placeholder_text("Enter your username");
        usernameEntry.set_width_chars(25);
        usernameEntry.set_hexpand(true);
        usernameEntry.signal_changed().connect([this]() {
            usernameEntry.get_style_context()->remove_class("error");
        });
        usernameEntry.signal_activate().connect([this]() {
            on_logInButton_clicked(); // so you don't have to click the button, how convenient!
        });
        grid.attach(usernameEntry, 1, 1, 2, 1);

        portHeader.set_text("P2P Port:");
        portHeader.set_halign(Align::ALIGN_END);
        grid.attach(portHeader, 0, 2, 1, 1);

        portEntry.set_placeholder_text("Port");
        portEntry.set_width_chars(5);
        portEntry.signal_changed().connect(sigc::mem_fun(*this, &LoginWindow::on_portEntry_changed));
        grid.attach(portEntry, 1, 2, 1, 1);

        autoPort.set_label("Auto assign port");
        autoPort.set_active(true);
        autoPort.signal_toggled().connect([this]() {
            portEntry.set_sensitive(!autoPort.get_active());
        });
        grid.attach(autoPort, 2, 2, 1, 1);

        grid.attach(bottom, 0, 4, 4, 1);
        bottom.set_row_spacing(10);
        bottom.set_column_spacing(10);

        rememberMe.set_label("Remember me");
        bottom.attach(rememberMe, 0, 0, 2, 1);

        logInButton.set_label("Log in");
        // logInButton.set_size_request(130, 20);
        logInButton.set_hexpand(true);
        logInButton.get_style_context()->add_class("suggested-action");
        logInButton.signal_clicked().connect(sigc::mem_fun(*this, &LoginWindow::on_logInButton_clicked));
        bottom.attach(logInButton, 0, 1, 1, 1);

        registerButton.set_label("Register");
        registerButton.signal_clicked().connect(sigc::mem_fun(*this, &LoginWindow::on_registerButton_clicked));
        bottom.attach(registerButton, 1, 1, 1, 1);

        show_all();
    }

private:
    void on_show() override {
        Window::on_show();

        clientAction.clientConfig.read();

        configureServerWindow.serverAddress = clientAction.clientConfig.serverAddress;
        configureServerWindow.port = clientAction.clientConfig.serverPort;

        usernameEntry.set_text(clientAction.clientConfig.username);
        portEntry.set_text(clientAction.clientConfig.p2pPort);

        rememberMe.set_active(!clientAction.clientConfig.username.empty());

        if (portEntry.get_text() == "0")
            autoPort.set_active(true);
        else
            autoPort.set_active(false);
        portEntry.set_sensitive(!autoPort.get_active());

        logInButton.set_sensitive(true);
        logInButton.set_label("Log in");

        usernameEntry.grab_focus();
    }

    void on_portEntry_changed() {
        portEntry.get_style_context()->remove_class("error");
        std::string port = portEntry.get_text();
        // Remove non-digit characters
        port.erase(std::remove_if(port.begin(), port.end(), [](char c) { return !std::isdigit(c); }), port.end());
        portEntry.set_text(port);

        if (port.empty()) {
            portEntry.get_style_context()->remove_class("error");
            return;
        }

        try {
            if (clientAction.p2pListenSocket.checkPort(port) == -1) {
                portEntry.get_style_context()->add_class("error");
            } else {
                portEntry.get_style_context()->remove_class("error");
            }
        } catch (const std::exception &) {
            portEntry.get_style_context()->add_class("error");
        }
    }

    void on_logInButton_clicked() {
        on_portEntry_changed();

        if (usernameEntry.get_text().empty()) {
            usernameEntry.get_style_context()->add_class("error");
            MessageDialog dialog(*this, "Username cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.run();
            return;
        }
        if (!autoPort.get_active()) {
            if (portEntry.get_text().empty()) {
                portEntry.get_style_context()->add_class("error");
                MessageDialog dialog(*this, "Port number cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
                dialog.run();
                return;
            }
            if (clientAction.p2pListenSocket.checkPort(portEntry.get_text()) == -1) {
                portEntry.get_style_context()->add_class("error");
                MessageDialog dialog(*this, "Invalid port number", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
                dialog.run();
                return;
            }
        }

        username = usernameEntry.get_text();
        clientPort = autoPort.get_active() ? "0" : portEntry.get_text();
        configureServerWindow.hide();

        logInButton.set_sensitive(false);
        logInButton.set_label("Logging in...");
        signal_timeout().connect_once(sigc::mem_fun(*this, &LoginWindow::logIn), 50);
    }

    void logIn() {
        clientAction.connectToServer(configureServerWindow.serverAddress, configureServerWindow.port);
        if (!clientAction.clientSocket.isConnected) {
            MessageDialog dialog(*this, "Failed to connect to server", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();

            // configureServerButton.get_style_context()->add_class("error");
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");
            return;
        }

        if (clientAction.logIn(username, clientPort)) {
            // login success
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");

            if (rememberMe.get_active()) {
                clientAction.clientConfig.username = username;
                clientAction.clientConfig.p2pPort = clientPort;
                clientAction.clientConfig.write(true);
            } else {
                clientAction.clientConfig.username = "";
                clientAction.clientConfig.p2pPort = "";
                clientAction.clientConfig.write(false); // next time the username and port will not be filled in
            }

            hide();

        } else {
            MessageDialog dialog(*this, "Login failed", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();
            logInButton.set_sensitive(true);
            logInButton.set_label("Log in");
        }
    }

    void on_registerButton_clicked() {
        registerWindow.serverAddress = configureServerWindow.serverAddress;
        registerWindow.port = configureServerWindow.port;
        registerWindow.set_transient_for(*this);
        registerWindow.show_all();
    }

    Grid top;
    Grid grid;
    Grid bottom;
    ScrolledWindow scrolledWindow;
    Label signInTitle;
    Button configureServerButton;
    Label usernameHeader;
    Entry usernameEntry;
    CheckButton rememberMe;
    Label portHeader;
    CheckButton autoPort;
    Entry portEntry;
    Button logInButton;
    Button registerButton;

    ConfigureServerWindow configureServerWindow;
    RegisterWindow registerWindow;
};

#endif // LOGIN_H