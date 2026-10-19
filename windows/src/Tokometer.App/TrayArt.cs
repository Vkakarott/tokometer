using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using Tokometer.Core;
using Forms = System.Windows.Forms;

namespace Tokometer.App;

/// <summary>
/// Draws the tray icon as a small gauge filled to the highest usage among the
/// visible providers, so a glance at the taskbar says how close a limit is.
/// </summary>
internal static partial class TrayArt
{
    private const int Size = 32;
    private static int? _shownLevel;

    public static void Apply(Forms.NotifyIcon icon, IReadOnlyList<ProviderCard> cards)
    {
        var highest = cards.SelectMany(card => card.Rows).Where(row => row.Known).Select(row => row.Fill).DefaultIfEmpty(-1).Max();
        // Redraw only when the gauge visibly changes: every 5%.
        var level = highest < 0 ? -1 : (int)Math.Round(highest * 20);
        if (level == _shownLevel && icon.Icon is not null) return;
        _shownLevel = level;
        var previous = icon.Icon;
        icon.Icon = Draw(highest);
        previous?.Dispose();
    }

    private static Icon Draw(double fill)
    {
        using var bitmap = new Bitmap(Size, Size);
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            var bounds = new Rectangle(4, 4, Size - 8, Size - 8);
            using var track = new Pen(Color.FromArgb(90, 255, 255, 255), 5);
            graphics.DrawArc(track, bounds, 135, 270);
            if (fill >= 0)
            {
                using var value = new Pen(ColorFor(fill), 5) { StartCap = LineCap.Round, EndCap = LineCap.Round };
                graphics.DrawArc(value, bounds, 135, (float)Math.Max(4, 270 * fill));
            }
        }
        var handle = bitmap.GetHicon();
        try
        {
            return (Icon)Icon.FromHandle(handle).Clone();
        }
        finally
        {
            DestroyIcon(handle);
        }
    }

    private static Color ColorFor(double fill) => fill switch
    {
        >= 0.9 => Color.FromArgb(0xEF, 0x44, 0x44),
        >= 0.7 => Color.FromArgb(0xF5, 0xA5, 0x24),
        _ => Color.FromArgb(0x2E, 0xD1, 0x6E),
    };

    [LibraryImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool DestroyIcon(IntPtr handle);
}
