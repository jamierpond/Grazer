#pragma once

#include <string_view>

namespace grazer
{
// Whether a Content-Disposition header tells the browser to save the response
// rather than show it ("attachment; filename=report.pdf").
bool isAttachmentDisposition(std::string_view header);
} // namespace grazer
