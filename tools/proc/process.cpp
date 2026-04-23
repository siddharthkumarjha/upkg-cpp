#include "stream-ranges.hpp"
#include "stream-stl.hpp"

#include "logging.hpp"
#include "result/result.hpp"
#include <charconv>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <gnu/libc-version.h>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <vector>

using namespace result_type;
namespace fs = std::filesystem;

namespace upkg
{
    struct CommandOutput
    {
        std::string stdout_;
        std::string stderr_;

        friend auto operator<<(std::ostream &oss, CommandOutput const &cmd_out) -> std::ostream &
        {
            return oss << "stdout: [" << cmd_out.stdout_ << "] stderr: [" << cmd_out.stderr_ << "]";
        }
    };

    enum class Stdio
    {
        Inherit,
        Null,
        MakePipe,
    };

    struct Process
    {
        using ExitStatus = int;
        using PidFd      = int;

        pid_t pid_;
        std::optional<ExitStatus> status_;

        std::optional<PidFd> pid_fd_;

        friend auto operator<<(std::ostream &oss, const Process &proc) -> std::ostream &
        {
            oss << "pid: " << proc.pid_ << " exit status: " << proc.status_ << " pidfd " << proc.pid_fd_;
            return oss;
        }
    };

    struct StdioPipes
    {
        using PipeFd = int;

        std::optional<PipeFd> stdin_;
        std::optional<PipeFd> stdout_;
        std::optional<PipeFd> stderr_;

        friend auto operator<<(std::ostream &oss, const StdioPipes &pipes) -> std::ostream &
        {
            return oss << "stdin: " << pipes.stdin_ << " stdout: " << pipes.stdout_ << " stderr: " << pipes.stderr_;
        }
    };

    struct Command
    {
        std::string program_;
        std::vector<std::string> args_;
        std::string cwd_;

        std::optional<Stdio> stdin_;
        std::optional<Stdio> stdout_;
        std::optional<Stdio> stderr_;

        Command(std::string &&program) : program_{std::move(program)} { args_.emplace_back(program); }

        friend auto operator<<(std::ostream &oss, const Command &cmd) -> std::ostream &
        {
            oss << "cmd: " << cmd.program_ << " " << cmd.args_;
            return oss;
        }

        auto spawn(Stdio const default_stdio, bool needs_stdin)
            -> Result<std::pair<Process, StdioPipes>, std::error_code>
        {
        }
    };

    auto which(std::string_view const binary_name) -> Result<fs::path, std::error_code>
    {
        const char ENV_KEY[] = "PATH";
        const char *env      = getenv(ENV_KEY);

        if (env == nullptr)
        {
            log::error("ENV var: ", ENV_KEY, " is not available");
            return Err(std::make_error_code(std::errc::no_such_process));
        }

        std::string_view const sv_env = env;
        auto sv_split =
            std::views::split(sv_env, ':') | std::views::transform(
                                                 [](auto &&sub_range) -> std::string_view
                                                 {
                                                     return {sub_range.begin(), std::ranges::size(sub_range)};
                                                 });

        for (const auto env_path : sv_split)
        {
            std::error_code ec;

            fs::path candidate = fs::path{env_path} / fs::path{binary_name};
            auto status        = fs::status(candidate, ec);
            if (ec == std::errc::no_such_file_or_directory)
            {
                continue;
            }
            else if (ec)
            {
                log::error(candidate, " couldn't be accessed, ec: ", ec.message());
                continue;
            }

            auto const perms            = status.permissions();
            bool universally_executable = ((perms & fs::perms::owner_exec) != fs::perms::none) &&
                                          ((perms & fs::perms::group_exec) != fs::perms::none) &&
                                          ((perms & fs::perms::others_exec) != fs::perms::none);

            if (status.type() != fs::file_type::regular)
            {
                log::warn(candidate, " found, but its not a regular file, skipping...");
            }
            else if (not universally_executable)
            {
                log::warn(candidate, " found, but its not universally executable, skipping...");
            }
            else
            {
                return Ok(fs::weakly_canonical(std::move(candidate)));
            }
        }

        log::error(binary_name, " not found in ", ENV_KEY);

        return Err(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    auto create_cmd(std::string_view const binary_name, std::span<std::string> const args)
        -> Result<Command, std::error_code>
    {
        auto full_path = TRY_OK(which(binary_name));
        auto cmd       = Command(std::move(full_path).string());
        cmd.args_.insert(cmd.args_.end(), std::make_move_iterator(args.begin()), std::make_move_iterator(args.end()));
        cmd.cwd_    = fs::current_path();

        cmd.stderr_ = Stdio::MakePipe;
        cmd.stdout_ = Stdio::MakePipe;
        cmd.stdin_  = Stdio::Null;
        return Ok(std::move(cmd));
    }

    auto glibc_version() -> std::optional<std::array<size_t, 2>>
    {
        const std::string_view version_str = gnu_get_libc_version();
        auto sv_split =
            std::views::split(version_str, '.') | std::views::transform(
                                                      [](auto &&sub_range) -> std::string_view
                                                      {
                                                          return {sub_range.begin(), std::ranges::size(sub_range)};
                                                      });
        const auto n = std::ranges::distance(sv_split);
        if (n != 2)
            return std::nullopt;

        std::array<size_t, 2> version_arr = {};
        size_t idx                        = 0;
        for (const auto ver_sub_str : sv_split)
        {
            if (std::from_chars(ver_sub_str.begin(), ver_sub_str.end(), version_arr[idx++]).ec != std::errc{})
                return std::nullopt;
        }
        return version_arr;
    }

    auto exec_cmd(std::string_view const binary_name, std::span<std::string> const args)
        -> Result<CommandOutput, std::error_code>
    {
        auto const cmd = TRY_OK(create_cmd(binary_name, args));
    }
} // namespace upkg
