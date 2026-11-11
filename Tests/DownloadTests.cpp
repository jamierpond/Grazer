#include <Download.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace grazer;

auto tAttachmentWithFilename =
    test("Download/attachment with a filename is a download") = []
{
    check(isAttachmentDisposition("attachment; filename=\"report.pdf\""));
    check(isAttachmentDisposition("attachment;filename*=UTF-8''r%C3%A9sum%C3%A9.pdf"));
};

auto tBareAttachment = test("Download/a bare attachment is a download") = []
{ check(isAttachmentDisposition("attachment")); };

auto tAttachmentIgnoresCaseAndSpaces =
    test("Download/the disposition type ignores case and surrounding spaces") = []
{
    check(isAttachmentDisposition("  Attachment ; filename=a.pdf"));
    check(isAttachmentDisposition("ATTACHMENT"));
};

auto tInlineIsNotDownload = test("Download/inline is shown, not saved") = []
{
    check(!isAttachmentDisposition("inline; filename=\"report.pdf\""));
    check(!isAttachmentDisposition("inline"));
};

auto tOtherHeadersAreNotDownloads =
    test("Download/empty, filename-only and look-alike types are not downloads") = []
{
    check(!isAttachmentDisposition(""));
    check(!isAttachmentDisposition("filename=attachment.pdf"));
    check(!isAttachmentDisposition("attachments"));
    check(!isAttachmentDisposition("form-data; name=attachment"));
};
