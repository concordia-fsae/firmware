** the script will set up the env for compiling - no need to run twice ** 
** to test new changes, simply enter container shell and execute scons - the files will update automatically due to mounitng feature ** 
** this also means that, once built, the build dir will be on your local machine as well. Run scons --clean in container if you want to re-build from scratch **

Local changes 
- Clone repo
	- clone with ssh link to enable ssh connection when pulling and pushing 
- Pull in submodules
	- git submodule update --init --recursive 
- Install and start docker (OS-specific)
- Install docker-compose 
- WINDOWS USERS: Install bash emulator: cygwin, mingw64, WSL (test optimal tool)
- Execute compile script from dev_container dir
	- ./buildroot.sh
		- change perms if necessary: chmod +x buildroot.sh

in container
-	scons --target=<component> 

