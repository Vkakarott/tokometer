using Microsoft.Data.Sqlite;
using Tokometer.Core.Providers;

namespace Tokometer.Core.Tests;

public class ProviderParsingTests
{
    [Fact]
    public void Codex_reads_the_five_hour_and_seven_day_windows()
    {
        var windows = CodexProvider.ParseWindows("""
            {"rate_limit":{
              "primary_window":{"used_percent":38,"reset_at":1790003600,"limit_window_seconds":18000},
              "secondary_window":{"used_percent":54,"reset_at":1790500000,"limit_window_seconds":604800}}}
            """);

        Assert.Equal(["five_hour", "seven_day"], windows.Select(w => w.Id));
        Assert.Equal(0.38, windows[0].UsedFraction!.Value, 3);
        Assert.Equal(DateTimeOffset.FromUnixTimeSeconds(1_790_003_600), windows[0].ResetsAt);
    }

    [Fact]
    public void Codex_skips_windows_of_an_unknown_length_or_without_values()
    {
        var windows = CodexProvider.ParseWindows("""
            {"rate_limit":{
              "primary_window":{"used_percent":10,"reset_at":1,"limit_window_seconds":3600},
              "secondary_window":{"reset_at":1,"limit_window_seconds":604800}}}
            """);

        Assert.Empty(windows);
    }

    [Fact]
    public void Codex_login_without_an_account_counts_as_signed_out()
    {
        using var temp = new TempDirectory();
        var file = temp.File("auth.json", """{"tokens":{"access_token":"abc"}}""");

        var error = Assert.Throws<ProviderFetchException>(() => CodexProvider.ReadLogin(file));
        Assert.Equal(FetchFailure.SignedOut, error.Failure);
    }

    [Fact]
    public void Cursor_reads_included_api_and_on_demand_usage()
    {
        var (windows, _) = CursorProvider.ParseUsage("""
            {"billingCycleEnd":"2026-11-01T00:00:00.010643+00:00","individualUsage":{
              "plan":{"totalPercentUsed":21,"apiPercentUsed":4},
              "onDemand":{"enabled":true,"used":5,"limit":40}}}
            """);

        Assert.Equal(["included", "api", "on_demand"], windows.Select(w => w.Id));
        Assert.Equal(0.125, windows[2].UsedFraction!.Value, 3);
        Assert.Equal(new DateTime(2026, 11, 1), windows[0].ResetsAt!.Value.UtcDateTime.Date);
    }

    [Fact]
    public void Cursor_leaves_out_zero_api_usage_and_disabled_on_demand()
    {
        var (windows, unlimited) = CursorProvider.ParseUsage("""
            {"isUnlimited":true,"individualUsage":{"plan":{"apiPercentUsed":0},"onDemand":{"enabled":false,"used":5,"limit":40}}}
            """);

        Assert.Empty(windows);
        Assert.True(unlimited);
    }

    [Fact]
    public void Cursor_token_comes_from_the_state_database()
    {
        using var temp = new TempDirectory();
        var database = temp.File("state.vscdb");
        CreateCursorState(database, "\"aaa.bbb.ccc\"");

        Assert.Equal("aaa.bbb.ccc", CursorProvider.ReadToken(database));
    }

    [Fact]
    public void Cursor_without_a_session_counts_as_signed_out()
    {
        using var temp = new TempDirectory();

        var error = Assert.Throws<ProviderFetchException>(() => CursorProvider.ReadToken(temp.File("missing.vscdb")));
        Assert.Equal(FetchFailure.SignedOut, error.Failure);
    }

    [Fact]
    public void Claude_reads_both_windows_with_fractional_reset_times()
    {
        var windows = ClaudeProvider.ParseWindows("""
            {"five_hour":{"utilization":86,"resets_at":"2026-09-22T21:30:00.010643+00:00"},
             "seven_day":{"utilization":19,"resets_at":"2026-09-27T00:00:00+00:00"}}
            """);

        Assert.Equal(["five_hour", "seven_day"], windows.Select(w => w.Id));
        Assert.Equal(0.86, windows[0].UsedFraction!.Value, 3);
    }

    [Fact]
    public void Claude_credentials_file_gives_the_access_token()
    {
        using var temp = new TempDirectory();
        var file = temp.File(".credentials.json", """{"claudeAiOauth":{"accessToken":"sk-ant-oat01-x"}}""");

        Assert.Equal("sk-ant-oat01-x", ClaudeProvider.ReadCredentialsToken(file));
        Assert.Null(ClaudeProvider.ReadCredentialsToken(temp.File("missing.json")));
    }

    private static void CreateCursorState(string database, string token)
    {
        using var connection = new SqliteConnection($"Data Source={database}");
        connection.Open();
        using var command = connection.CreateCommand();
        command.CommandText = "CREATE TABLE ItemTable (key TEXT, value TEXT); INSERT INTO ItemTable VALUES ('cursorAuth/accessToken', $token);";
        command.Parameters.AddWithValue("$token", token);
        command.ExecuteNonQuery();
        SqliteConnection.ClearAllPools();
    }
}
