#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/logging/Message.h"

namespace QtRocket
{

/// A user-visible error (OpenRocket: logging/Error; renamed because QtRocket::Error is the
/// failure type of Result<T>). The only concrete error is the text-only Other, as in OpenRocket.
class ErrorMessage : public Message
{
public:
    ~ErrorMessage() override = default;

    class Other;

    /// An error with that text (Java: fromString()).
    [[nodiscard]] static Other fromString(std::string text);

protected:
    ErrorMessage()                                   = default;
    ErrorMessage(const ErrorMessage&)                = default;
    ErrorMessage(ErrorMessage&&) noexcept            = default;
    ErrorMessage& operator=(const ErrorMessage&)     = default;
    ErrorMessage& operator=(ErrorMessage&&) noexcept = default;
};

/// An error that is only its text (Java: Error.Other). Unlike Warning::Other, two of these are
/// equal when their texts are equal, whatever their priorities and sources (Java compares only the
/// description here).
class ErrorMessage::Other final : public ErrorMessage
{
public:
    explicit Other(std::string description);

    [[nodiscard]] const std::string& description() const noexcept { return m_description; }

    [[nodiscard]] std::string messageDescription() const override { return m_description; }
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view         typeName() const noexcept override { return "Other"; }
    [[nodiscard]] bool                     equals(const Message& other) const override;

private:
    std::string m_description;
};

}  // namespace QtRocket
