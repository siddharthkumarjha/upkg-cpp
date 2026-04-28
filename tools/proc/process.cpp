#include "stream-ranges.hpp"
#include "stream-stl.hpp"

#include "defer.hpp"
#include "logging.hpp"
#include "result/result.hpp"
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <ranges>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using namespace result_type;
using namespace std::literals;

namespace fs = std::filesystem;

extern "C" char **environ;

namespace upkg
{
    enum ExitStatusKind
    {
        Continued,
        Dumped,
        Exited,
        Killed,
        Stopped,
        Trapped,
        Uncategorized,
    };

    static inline auto to_sv(ExitStatusKind const exit) -> std::string_view
    {
        static constexpr std::string_view sv_arr[] = {
            [ExitStatusKind::Continued]     = "Continued",
            [ExitStatusKind::Dumped]        = "Dumped",
            [ExitStatusKind::Exited]        = "Exited",
            [ExitStatusKind::Killed]        = "Killed",
            [ExitStatusKind::Stopped]       = "Stopped",
            [ExitStatusKind::Trapped]       = "Trapped",
            [ExitStatusKind::Uncategorized] = "Uncategorized",
        };
        if (exit >= ExitStatusKind::Continued and exit <= ExitStatusKind::Uncategorized)
            return sv_arr[exit];
        return "UNKNOWN"sv;
    }

    static inline auto raw_to_exit_kind(const int raw) -> ExitStatusKind
    {

        switch (raw)
        {
        case CLD_CONTINUED:
            return ExitStatusKind::Continued;
        case CLD_DUMPED:
            return ExitStatusKind::Dumped;
        case CLD_EXITED:
            return ExitStatusKind::Exited;
        case CLD_KILLED:
            return ExitStatusKind::Killed;
        case CLD_STOPPED:
            return ExitStatusKind::Stopped;
        case CLD_TRAPPED:
            return ExitStatusKind::Trapped;
        default:
            return ExitStatusKind::Uncategorized;
        }
    }

    struct ExitStatus
    {
        int value;
        ExitStatusKind kind;

        ExitStatus(siginfo_t const &process_info)
            : value{process_info.si_status}, kind{raw_to_exit_kind(process_info.si_code)}
        {
        }

        friend auto operator<<(std::ostream &oss, const ExitStatus &status) -> std::ostream &
        {
            return oss << "value: " << status.value << " kind: " << to_sv(status.kind);
        }
    };

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

        StdioPipes(std::optional<PipeFd> stdin_fd = std::nullopt, std::optional<PipeFd> stdout_fd = std::nullopt,
                   std::optional<PipeFd> stderr_fd = std::nullopt)
            : stdin_{stdin_fd}, stdout_{stdout_fd}, stderr_{stderr_fd} {};

        StdioPipes(StdioPipes const &)            = delete;
        StdioPipes &operator=(StdioPipes const &) = delete;

        StdioPipes(StdioPipes &&)                 = default;
        StdioPipes &operator=(StdioPipes &&)      = default;

        ~StdioPipes()                             = default;
        // {
        //     if (stdin_.has_value())
        //         close(stdin_.value());
        //
        //     if (stdout_.has_value())
        //         close(stdout_.value());
        //
        //     if (stderr_.has_value())
        //         close(stderr_.value());
        // }

        friend auto operator<<(std::ostream &oss, const StdioPipes &pipes) -> std::ostream &
        {
            return oss << "stdin: " << pipes.stdin_ << " stdout: " << pipes.stdout_ << " stderr: " << pipes.stderr_;
        }
    };

    struct ChildPipes : public StdioPipes
    {
        using StdioPipes::StdioPipes;
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

        auto spawn(Stdio const default_stdio = Stdio::Inherit, bool const needs_stdin = false)
            -> Result<std::pair<Process, StdioPipes>, std::error_code>
        {
            auto [ours, theirs]        = TRY_OK(setup_io(default_stdio, needs_stdin));

            using PosixSpwanAddChdirFn = int (*)(posix_spawn_file_actions_t *__restrict, const char *__restrict);
            auto const addchdir = !cwd_.empty() ? std::make_optional<std::pair<PosixSpwanAddChdirFn, std::string_view>>(
                                                      posix_spawn_file_actions_addchdir_np, cwd_)
                                                : std::nullopt;

            posix_spawnattr_t attr = {0};
            TRY_OK(cvt_nz(posix_spawnattr_init(&attr)));
            DEFER[&] { posix_spawnattr_destroy(&attr); };

            posix_spawn_file_actions_t file_actions = {0};
            TRY_OK(cvt_nz(posix_spawn_file_actions_init(&file_actions)));
            DEFER[&] { posix_spawn_file_actions_destroy(&file_actions); };

            if (theirs.stdin_.has_value())
            {
                TRY_OK(cvt_nz(posix_spawn_file_actions_adddup2(&file_actions, *theirs.stdin_, STDIN_FILENO)));
            }
            if (theirs.stdout_.has_value())
            {
                TRY_OK(cvt_nz(posix_spawn_file_actions_adddup2(&file_actions, *theirs.stdout_, STDOUT_FILENO)));
            }
            if (theirs.stderr_.has_value())
            {
                TRY_OK(cvt_nz(posix_spawn_file_actions_adddup2(&file_actions, *theirs.stderr_, STDERR_FILENO)));
            }

            if (addchdir.has_value())
            {
                auto const &[f, cwd] = *addchdir;
                TRY_OK(cvt_nz(f(&file_actions, cwd.data())));
            }

            sigset_t default_set = {};
            TRY_OK(cvt(sigemptyset(&default_set)));
            TRY_OK(cvt(sigaddset(&default_set, SIGPIPE)));
            TRY_OK(cvt_nz(posix_spawnattr_setsigdefault(&attr, &default_set)));

            TRY_OK(cvt_nz(posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF)));

            int pid_fd      = -1;
            auto args_range = args_ | std::views::transform(
                                          [](std::string &arg_str) -> char *
                                          {
                                              return arg_str.data();
                                          });

            std::vector<char *> args{args_range.begin(), args_range.end()};
            args.emplace_back(nullptr);

            TRY_OK(cvt_nz(pidfd_spawnp(&pid_fd, program_.c_str(), &file_actions, &attr, args.data(), environ)));
            // auto const pid = pidfd_getpid(pid_fd);
            // if (pid == -1)
            // {
            //     close(pid_fd);
            //     return Err(std::make_error_code(static_cast<std::errc>(errno)));
            // }

            return Ok(std::make_pair(
                Process{.pid_    = -1,
                        .status_ = std::nullopt,
                        .pid_fd_ = (pid_fd >= 0) ? std::make_optional<Process::PidFd>(pid_fd) : std::nullopt},
                std::move(ours)));
        }

    private:
        static inline auto cvt(const int error) -> Result<void, std::error_code>
        {
            return (error == -1) ? make_err<void, std::error_code>(std::make_error_code(static_cast<std::errc>(errno)))
                                 : make_ok<void, std::error_code>();
        }

        static inline auto cvt_nz(const int error) -> Result<void, std::error_code>
        {
            return (error == 0) ? make_ok<void, std::error_code>()
                                : make_err<void, std::error_code>(std::make_error_code(static_cast<std::errc>(error)));
        }

        auto setup_io(Stdio const default_stdio, bool const needs_stdin) const
            -> Result<std::pair<StdioPipes, ChildPipes>, std::error_code>
        {
            auto const default_stdin               = needs_stdin ? default_stdio : Stdio::Null;

            auto ours                              = StdioPipes{};
            auto theirs                            = ChildPipes{};

            std::tie(theirs.stdin_, ours.stdin_)   = TRY_OK(to_child_stdio(stdin_.value_or(default_stdin), true));
            std::tie(theirs.stdout_, ours.stdout_) = TRY_OK(to_child_stdio(stdout_.value_or(default_stdio), false));
            std::tie(theirs.stderr_, ours.stderr_) = TRY_OK(to_child_stdio(stderr_.value_or(default_stdio), false));

            return Ok(std::make_pair(std::move(ours), std::move(theirs)));
        }

        // clang-format off
        auto static inline to_child_stdio(const Stdio stdio, const bool readable)
            -> Result<
                    std::pair
                    <
                        std::optional<ChildPipes::PipeFd>,
                        std::optional<StdioPipes::PipeFd>
                    >, 
                    std::error_code>
        {
            switch (stdio)
            {
            case Stdio::Inherit:
            {
                return Ok<
                    std::pair
                    <
                        std::optional<ChildPipes::PipeFd>,
                        std::optional<StdioPipes::PipeFd>
                    >>
                    (std::make_pair(std::nullopt, std::nullopt));
            }
            break;
            case Stdio::Null:
            {
                const mode_t rw             = readable ? O_RDONLY : O_WRONLY;
                const ChildPipes::PipeFd fd = open("/dev/null", O_CLOEXEC | rw);
                TRY_OK(cvt(fd));
                    
                return Ok<
                    std::pair
                    <
                        std::optional<ChildPipes::PipeFd>,
                        std::optional<StdioPipes::PipeFd>
                    >>
                    (std::make_pair(fd, std::nullopt));
            }
            break;
            case Stdio::MakePipe:
            {
                int fds[2] = {0};
                TRY_OK(cvt(pipe2(fds, O_CLOEXEC)));

                auto const &[reader, writer] = fds;
                auto const [ours, theirs] = readable ? std::tie(writer, reader) : std::tie(reader, writer);

                return Ok<
                    std::pair
                    <
                        std::optional<ChildPipes::PipeFd>,
                        std::optional<StdioPipes::PipeFd>
                    >>
                    (std::make_pair(theirs, ours));
            }
            break;
            }

            return Err(std::make_error_code(std::errc::invalid_argument));
        }
        // clang-format on
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

    static inline auto check_syscall(const int result) -> Result<void, std::error_code>
    {
        return (result >= 0) ? make_ok<void, std::error_code>()
                             : make_err<void, std::error_code>(std::make_error_code(static_cast<std::errc>(errno)));
    }

    auto exec_cmd(std::string_view const binary_name, std::span<std::string> const args)
        -> Result<CommandOutput, std::error_code>
    {
        auto cmd                     = TRY_OK(create_cmd(binary_name, args));
        auto const [proc, our_pipes] = TRY_OK(cmd.spawn());

        if (proc.pid_fd_.has_value())
        {
            siginfo_t process_info = {};
            TRY_OK(check_syscall(waitid(P_PIDFD, *proc.pid_fd_, &process_info, WEXITED | WNOWAIT | WSTOPPED)));
            log::info("ExitStatus:: ", ExitStatus(process_info));
        }
        else
        {
            log::error("Unexpected pid_fd is not available!");
            return Err(std::make_error_code(std::errc::bad_file_descriptor));
        }

        log::info("done with all the proc stuff!");
        exit(0);
    }
} // namespace upkg
