#include "gdtf_share_message_formatter.h"

#include <wx/translation.h>

namespace {

// Returns a complete operation-specific message without translated fragments.
wxString OperationMessage(GdtfShareGuiOperation operation,
                          const wxString &loginMessage,
                          const wxString &downloadMessage) {
  return operation == GdtfShareGuiOperation::Login ? loginMessage
                                                   : downloadMessage;
}

} // namespace

// Formats a complete localized GUI message from a structured GDTF Share result.
wxString FormatLocalizedGdtfShareUserMessage(
    const GdtfShareResult &result, GdtfShareGuiOperation operation) {
  switch (result.category) {
  case GdtfShareResultCategory::Success:
    return OperationMessage(operation, _("GDTF Share sign-in completed."),
                            _("GDTF Share download completed."));
  case GdtfShareResultCategory::TransportError:
    return wxString::Format(_("Could not reach GDTF Share: %s."),
                            wxString::FromUTF8(result.transportMessage));
  case GdtfShareResultCategory::Timeout:
    return OperationMessage(operation, _("GDTF Share sign-in timed out."),
                            _("GDTF Share download timed out."));
  case GdtfShareResultCategory::AuthenticationRejected:
    return wxString::Format(
        _("The GDTF Share username or password is invalid (HTTP %ld)."),
        result.httpStatus);
  case GdtfShareResultCategory::ApiRejected:
    if (result.httpStatus != 0) {
      return OperationMessage(
          operation,
          wxString::Format(
              _("GDTF Share rejected the sign-in request (HTTP %ld)."),
              result.httpStatus),
          wxString::Format(
              _("GDTF Share rejected the download request (HTTP %ld)."),
              result.httpStatus));
    }
    return OperationMessage(operation,
                            _("GDTF Share rejected the sign-in request."),
                            _("GDTF Share rejected the download request."));
  case GdtfShareResultCategory::HttpError:
    return OperationMessage(
        operation,
        wxString::Format(_("GDTF Share returned HTTP %ld during sign-in."),
                         result.httpStatus),
        wxString::Format(_("GDTF Share returned HTTP %ld during download."),
                         result.httpStatus));
  case GdtfShareResultCategory::InvalidJsonResponse:
  case GdtfShareResultCategory::InvalidResponseSchema:
    return _("GDTF Share returned an invalid response. See the diagnostic log for details.");
  case GdtfShareResultCategory::Cancelled:
    return OperationMessage(operation, _("GDTF Share sign-in was cancelled."),
                            _("GDTF Share download was cancelled."));
  case GdtfShareResultCategory::LocalFileError:
    return _("Could not save the GDTF Share download locally.");
  }
  return OperationMessage(operation, _("GDTF Share sign-in failed."),
                          _("GDTF Share download failed."));
}
