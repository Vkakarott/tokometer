using System.Security.Cryptography;

namespace Tokometer.App;

/// <summary>Windows DPAPI for the signed-in user: what Chromium apps protect their cookie key with.</summary>
internal static class Dpapi
{
    public static byte[] Unprotect(byte[] data) => ProtectedData.Unprotect(data, null, DataProtectionScope.CurrentUser);

    public static byte[] Protect(byte[] data) => ProtectedData.Protect(data, null, DataProtectionScope.CurrentUser);
}
