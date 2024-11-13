#ifndef PAY_H
#define PAY_H

#include <gtkmm.h>
#include "client_action.h"

using namespace Glib;
using namespace Gtk;

class PayWindow : public Window {
public:
    std::string payeeUsername;

    PayWindow() {
        set_title("Send Micropayment");
        set_default_size(200, 100);

        grid.set_row_spacing(10);
        grid.set_column_spacing(10);
        grid.set_margin_top(15);
        grid.set_margin_bottom(15);
        grid.set_margin_start(15);
        grid.set_margin_end(15);

        add(grid);

        payeeLabel.set_text("Payee: ");
        payeeLabel.set_halign(Align::ALIGN_END);
        grid.attach(payeeLabel, 0, 0, 1, 1);

        payeeUsernameEntry.set_placeholder_text("Payee username");
        payeeUsernameEntry.signal_changed().connect(sigc::mem_fun(*this, &PayWindow::on_payeeUsernameEntry_changed));
        payeeUsernameEntry.set_text(payeeUsername);
        grid.attach(payeeUsernameEntry, 1, 0, 1, 1);

        amountLabel.set_text("Amount: ");
        amountLabel.set_halign(Align::ALIGN_END);
        grid.attach(amountLabel, 0, 1, 1, 1);

        amountEntry.set_placeholder_text("Amount");
        amountEntry.signal_changed().connect(sigc::mem_fun(*this, &PayWindow::on_amountEntry_changed));
        grid.attach(amountEntry, 1, 1, 1, 1);

        payButton.set_label("Pay");
        payButton.get_style_context()->add_class("suggested-action");
        payButton.signal_clicked().connect(sigc::mem_fun(*this, &PayWindow::on_payButton_clicked));
        grid.attach(payButton, 0, 2, 2, 1);

        show_all_children();
    }

    void on_show() override {
        Window::on_show();
        payeeUsernameEntry.set_text(payeeUsername);
        amountEntry.set_text("");
        payButton.get_style_context()->remove_class("success");
        payButton.get_style_context()->add_class("suggested-action");
        payButton.set_label("Pay");

        amountEntry.grab_focus();

        show_all_children();
    }

    void on_payButton_clicked() {
        if (payeeUsername.empty()) {
            payeeUsernameEntry.get_style_context()->add_class("error");
            MessageDialog dialog(*this, "Payee username cannot be empty", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            return;
        }
        bool usernameExists = false;
        for (auto& user : clientAction.userAccounts) {
            if (user.username == payeeUsername) {
                usernameExists = true;
                break;
            }
        }
        if (!usernameExists) {
            payeeUsernameEntry.get_style_context()->add_class("error");
            MessageDialog dialog(*this, "Payee username not found", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            return;
        }

        int amount = 0;
        try {
            amount = std::stoi(amountEntry.get_text());
        } catch (const std::exception&) {
            amountEntry.get_style_context()->add_class("error");
            std::cerr << "Failed to convert payment amount to a number" << std::endl;
            return;
        }
        if (amount <= 0) {
            amountEntry.get_style_context()->add_class("error");
            MessageDialog dialog(*this, "Invalid payment amount", false, MessageType::MESSAGE_ERROR, ButtonsType::BUTTONS_OK, true);
            return;
        }

        MessageDialog dialog(*this, "Confirm payment", false, MessageType::MESSAGE_QUESTION, ButtonsType::BUTTONS_OK, true);
        dialog.set_secondary_text("Are you sure you want to send " + amountEntry.get_text() + " to " + payeeUsernameEntry.get_text() + "?\nThis action cannot be undone.");
        int result = dialog.run();
        if (result == RESPONSE_OK && (clientAction.sendMicropaymentTransaction(amount, payeeUsername))) {
            amountEntry.set_text("");
            payButton.get_style_context()->remove_class("suggested-action");
            payButton.get_style_context()->add_class("success");
            payButton.set_label("Payment sent!");
        }
    }
    
    void on_payeeUsernameEntry_changed() {
        payButton.get_style_context()->remove_class("success");
        payButton.get_style_context()->add_class("suggested-action");
        payButton.set_label("Pay");

        payeeUsername = payeeUsernameEntry.get_text();

        if (payeeUsername.empty()) {
            payeeUsernameEntry.get_style_context()->add_class("error");
        } else {
            payeeUsernameEntry.get_style_context()->remove_class("error");
        }

        for (auto& user : clientAction.userAccounts) {
            if (user.username == payeeUsername) {
                payeeUsernameEntry.get_style_context()->remove_class("error");
                break;
            }
            payeeUsernameEntry.get_style_context()->add_class("error");
        }
    }

    void on_amountEntry_changed() {
        payButton.get_style_context()->remove_class("success");
        payButton.get_style_context()->add_class("suggested-action");
        payButton.set_label("Pay");

        std::string amount = amountEntry.get_text();
        amount.erase(std::remove_if(amount.begin(), amount.end(), [](char c) { return !std::isdigit(c); }), amount.end());
        amountEntry.set_text(amount);

        try {
            if (amount.empty() || std::stoi(amount) <= 0) {
                amountEntry.get_style_context()->remove_class("warning");
                amountEntry.get_style_context()->add_class("error");
            } else if (std::stoi(amount) > clientAction.accountBalance) {
                amountEntry.get_style_context()->add_class("warning");
            } else {
                amountEntry.get_style_context()->remove_class("error");
                amountEntry.get_style_context()->remove_class("warning");
            }
        } catch (const std::exception&) {
            amountEntry.get_style_context()->add_class("error");
            std::cerr << "Failed to convert payment amount to a number" << std::endl;
        }
    }

private:
    Grid grid;
    Label payeeLabel;
    Entry payeeUsernameEntry;
    Label amountLabel;
    Entry amountEntry;

    Button payButton;

};

#endif // PAY_H