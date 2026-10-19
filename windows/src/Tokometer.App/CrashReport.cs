using System.IO;
using System.Windows;

namespace Tokometer.App;

/// <summary>
/// A tray app has no window to die in, so a failure would otherwise close it
/// without a trace. Writes the error to a log and tells the user where it is.
/// </summary>
internal static class CrashReport
{
    private static readonly string LogFile = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Tokometer", "erro.log");

    public static void Show(Exception? error)
    {
        var written = TryWrite(error);
        var where = written ? $"\n\nDetalhes em:\n{LogFile}" : "";
        MessageBox.Show(
            $"O Tokometer encontrou um erro e vai fechar.\n\n{error?.Message}{where}",
            "Tokometer", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private static bool TryWrite(Exception? error)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(LogFile)!);
            File.AppendAllText(LogFile, $"[{DateTimeOffset.Now:O}] {error}\n\n");
            return true;
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }
}
