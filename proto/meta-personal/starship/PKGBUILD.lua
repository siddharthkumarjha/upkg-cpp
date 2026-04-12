local pkg_info = {
	name = "starship",
	desc = "The cross-shell prompt for astronauts",
	ver = "1.23.0",
}

local github_link = "https://github.com" .. "/" .. pkg_info.name .. "/" .. pkg_info.name
local pkg_tag = "v" .. pkg_info.ver

Package = {
	pkg = pkg_info,

	url = "https://starship.rs/",
	license = { "ISC" },
	depends = { "gcc-libs", "glibc" },
	make_depends = { "cmake", "git", "rust" },
	check_depends = { "python" },
	opt_depends = { { name = "ttf-font-nerd", desc = "Nerd Font Symbols preset" } },
	-- or alternative
	-- opt_depends = { "ttf-font-nerd" },

	source = {
		{
			proto = Proto.git,
			url = github_link,
			tag = pkg_tag,
			repo_name = "starship",
		},
		{ proto = Proto.file, file = "./0001-fix-rust-1.89.0-warnings-and-errors-blocking-CI-pipe.patch" },
		{ proto = Proto.file, file = "./0002-fix-git-tests-spawning-an-editor.patch" },
	},

	checksum = {
		Skip,
		{ kind = CheckSumKind.sha256, digest = "2e66eff0249f87f1deb1dfd0916d1017c1772a05a7627668d8855a3f227908e8" },
		{ kind = CheckSumKind.sha256, digest = "41e085267c1a8c60b29442a8376c4cf2c1f98f658b13ff17370887413047e7f4" },
	},
}

function Run(cmd)
	local handle = io.popen(cmd .. " 2>&1")
	local output = handle:read("*a")
	handle:close()
	return output
end

function GetHost(rustc_output)
	return string.match(rustc_output, "host:%s*(%S+)")
end

function Prepare()
	for _, s in ipairs(Package.source) do
		if s.proto == Proto.file then
			local patch_cmd = "patch" .. " -d starship -p1 < " .. s.file
			print(patch_cmd)
			os.execute(patch_cmd)
		end
	end

	local rust_version = Run("rustc -vV")
	local target_host = GetHost(rust_version)

	local cargo_cmd = "cargo fetch --locked --target " .. target_host .. " --manifest-path starship/Cargo.toml"
	print(cargo_cmd)
	os.execute(cargo_cmd)
end

function Build()
	local env = 'CARGO_TARGET_DIR=target CFLAGS+=" -ffat-lto-objects"'
	local cargo_cmd = "cargo build --release --frozen --manifest-path starship/Cargo.toml"
	print(cargo_cmd)
	os.execute(env .. " " .. cargo_cmd)
end

function Check()
	local cargo_cmd = "cargo test --frozen --manifest-path starship/Cargo.toml"
	print(cargo_cmd)
	os.execute(cargo_cmd)
end

function Install()
	local install_starship = table.concat({
		"install -Dm 755 target/release/starship -t " .. InstallDir .. "/usr/bin",
		"install -Dm 644 starship/LICENSE -t " .. InstallDir .. "/usr/share/licenses/starship/",
		"install -dm 755 "
		.. InstallDir
		.. "/usr/share/{bash-completion/completions,elvish/lib,fish/vendor_completions.d,zsh/site-functions}/",
		"./target/release/starship completions bash > "
		.. InstallDir
		.. "/usr/share/bash-completion/completions/starship",
		"./target/release/starship completions elvish > " .. InstallDir .. "/usr/share/elvish/lib/starship.elv",
		"./target/release/starship completions fish > "
		.. InstallDir
		.. "/usr/share/fish/vendor_completions.d/starship.fish",
		"./target/release/starship completions zsh > " .. InstallDir .. "/usr/share/zsh/site-functions/_starship",
	}, "\n")
	print(install_starship)
	os.execute(install_starship)
end

local function get_filename_from_url(url)
	-- Strip query string and fragment
	local cleaned = url:match("^[^?#]+")
	-- Extract last part after final slash
	return cleaned:match("^.+/(.+)$")
end

function Verify()
	for i, s in ipairs(Package.source) do
		local file_name
		if s.file ~= nil then
			file_name = s.file
		elseif s.url ~= nil then
			file_name = get_filename_from_url(s.url)
		else
			error("no file passed to source?")
		end

		if Package.checksum[i] ~= Skip then
			local actual_sha = Run(Package.checksum[i].kind .. "sum " .. file_name):match("^([a-f0-9]+)")
			if actual_sha ~= Package.checksum[i].digest then
				error(
					"CheckSum "
					.. Package.checksum[i].kind
					.. " mismatch, expected: "
					.. Package.checksum[i].digest
					.. " got "
					.. actual_sha
					.. " for file "
					.. file_name
				)
			else
				print(Package.checksum[i].kind .. " for " .. file_name .. " successfully validated")
			end
		else
			print("for ", file_name, " SKIP sha check requested")
		end
	end
end

function Fetch()
	for _, s in ipairs(Source) do
		if s.proto == Proto.git then
			local clone_cmd = s.proto .. " clone " .. s.url .. " -b " .. s.tag .. " " .. s.directory
			print(clone_cmd)
			os.execute(clone_cmd)
		end
	end
end

-- control flow
-- download source -> verify() -> extract source -> prepare() -> build() -> check() -> install()
function Test_fn()
	Fetch()
	Verify()
	Prepare()
	Build()
	Check()
	Install()
end
