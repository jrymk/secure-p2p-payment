#ifndef REGISTER_H
#define REGISTER_H

#include <gtkmm.h>
#include "../clientAction.h"

using namespace Glib;
using namespace Gtk;

class RegisterWindow : public Window {
public:
    std::string serverAddress = "localhost";
    std::string port = "5050";

    RegisterWindow() {
        set_title("Register");
        set_default_size(300, 100);

        grid.set_row_spacing(10);
        grid.set_column_spacing(10);

        grid.set_margin_top(15);
        grid.set_margin_bottom(15);
        grid.set_margin_start(15);
        grid.set_margin_end(15);

        add(grid);

        registerTitle.set_text("Register");
        Pango::FontDescription textFont;
        textFont.set_size(16 * PANGO_SCALE);
        textFont.set_weight(Pango::Weight::WEIGHT_BOLD);
        registerTitle.override_font(textFont);
        registerTitle.set_halign(Align::ALIGN_START);
        grid.attach(registerTitle, 0, 0, 2, 1);

        usernameHeading.set_text("Username:");
        grid.attach(usernameHeading, 0, 1, 1, 1);

        usernameEntry.set_placeholder_text("Username");
        usernameEntry.signal_changed().connect([this]() {
            usernameEntry.get_style_context()->remove_class("error");
        });
        usernameEntry.signal_activate().connect([this]() {
            on_registerButton_clicked();
        });
        grid.attach(usernameEntry, 1, 1, 1, 1);

        registerButton.set_label("Register");
        registerButton.get_style_context()->add_class("suggested-action");
        registerButton.signal_clicked().connect(sigc::mem_fun(*this, &RegisterWindow::on_registerButton_clicked));
        grid.attach(registerButton, 0, 2, 2, 1);

        show_all_children();
    }

    void on_show() override {
        Window::on_show();
        usernameEntry.set_text("");
        registerButton.set_sensitive(true);
        registerButton.set_label("Register");
        usernameEntry.grab_focus();
    }

    void on_registerButton_clicked() {
        if (usernameEntry.get_text().empty()) {
            MessageDialog dialog(*this, "Username cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.run();
            return;
        }

        registerButton.set_sensitive(false);
        registerButton.set_label("Registering...");
        signal_timeout().connect_once(sigc::bind(sigc::mem_fun(*this, &RegisterWindow::registerAccount), usernameEntry.get_text()), 50);
    }

    void registerAccount(const std::string &username) {
        clientAction.connectToServer(serverAddress, port);
        if (!clientAction.clientSocket.isConnected) {
            MessageDialog dialog(*this, "Failed to connect to server", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();
            registerButton.set_sensitive(true);
            registerButton.set_label("Register");
            return;
        }

        if (clientAction.registerAccount(username)) {
            MessageDialog dialog(*this, "Registration successful", false, MessageType::MESSAGE_INFO, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text("You may now log in.\n" + clientAction.error_t);
            dialog.run();
            hide();
        } else {
            MessageDialog dialog(*this, "Registration failed", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            dialog.set_secondary_text(clientAction.error_t);
            dialog.run();
        }
        registerButton.set_sensitive(true);
        registerButton.set_label("Register");
    }

private:
    Grid grid;
    Label registerTitle;
    Label usernameHeading;
    Entry usernameEntry;
    Button registerButton;
};

#endif // REGISTER_H