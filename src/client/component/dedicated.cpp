#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "game/game.hpp"
#include "game/dvars.hpp"

#include "dvars.hpp"
#include "command.hpp"
#include "console.hpp"
#include "scheduler.hpp"

#include <utils/hook.hpp>
#include <utils/string.hpp>
#include <utils/flags.hpp>

namespace dedicated
{
	namespace
	{
		std::vector<std::string>& get_startup_command_queue()
		{
			static std::vector<std::string> startup_command_queue;
			return startup_command_queue;
		}

		void execute_startup_command(int client, int /*controllerIndex*/, const char* command)
		{
			if (game::Live_SyncOnlineDataFlags(0) == 0)
			{
				game::Cbuf_ExecuteBufferInternal(0, 0, command, game::Cmd_ExecuteSingleCommand);
			}
			else
			{
				get_startup_command_queue().emplace_back(command);
			}
		}

		void execute_startup_command_queue()
		{
			const auto queue = get_startup_command_queue();
			get_startup_command_queue().clear();

			for (const auto& command : queue)
			{
				game::Cbuf_ExecuteBufferInternal(0, 0, command.data(), game::Cmd_ExecuteSingleCommand);
			}
		}

		bool party_is_server_dedicated_stub()
		{
			return true;
		}

		void sync_gpu_stub()
		{
			std::this_thread::sleep_for(1ms);
		}

		void gscr_is_using_match_rules_data_stub()
		{
			game::Scr_AddInt(0);
		}

		void init_dedicated_server()
		{
			// R_RegisterDvars
			utils::hook::invoke<void>(0xDF62C0_b);

			// R_RegisterCmds
			utils::hook::invoke<void>(0xDD7E50_b);

			// RB_Tonemap_RegisterDvars
			utils::hook::invoke<void>(0x4B1320_b);

			static bool initialized = false;
			if (initialized) return;
			initialized = true;

			// R_LoadGraphicsAssets
			utils::hook::invoke<void>(0xE06220_b);
		}
	}

	void initialize()
	{
		command::execute("onlinegame 1", true);
		command::execute("xblive_privatematch 1", true);
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			if (!game::environment::is_dedi())
			{
				return;
			}

#ifdef DEBUG
			printf("Starting dedicated server\n");
#endif
			
			// Register dedicated dvar
			game::Dvar_RegisterBool("dedicated", true, game::DVAR_FLAG_READ, "Dedicated server");

			// Add lanonly mode
			game::Dvar_RegisterBool("sv_lanOnly", false, game::DVAR_FLAG_NONE, "Don't send heartbeat");

			// Disable frontend
			dvars::override::register_bool("frontEndSceneEnabled", false, game::DVAR_FLAG_READ);

			// Disable shader preload
			dvars::override::register_bool("r_preloadShaders", false, game::DVAR_FLAG_READ);

			// Disable load for renderer
			dvars::override::register_bool("r_loadForRenderer", false, game::DVAR_FLAG_READ);

			dvars::override::register_bool("intro", false, game::DVAR_FLAG_READ);

			// Is party dedicated
			utils::hook::jump(0x5DFC10_b, party_is_server_dedicated_stub);

			// Make GScr_IsUsingMatchRulesData return 0 so the game doesn't override the cfg
			utils::hook::jump(0xB53950_b, gscr_is_using_match_rules_data_stub);

			// Hook R_SyncGpu
			utils::hook::jump(0xE08AE0_b, sync_gpu_stub, true);

			utils::hook::jump(0x341B60_b, init_dedicated_server, true);

			// delay startup commands until the initialization is done
			utils::hook::call(0xB8D20F_b, execute_startup_command);

			utils::hook::nop(0xCDD5D3_b, 5); // don't load config file
			utils::hook::nop(0xB7CE46_b, 5); // ^
			utils::hook::set<uint8_t>(0xBB0930_b, 0xC3); // don't save config file

			utils::hook::set<uint8_t>(0x9D49C0_b, 0xC3); // disable self-registration
			utils::hook::set<uint8_t>(0xE574E0_b, 0xC3); // render thread
			utils::hook::set<uint8_t>(0x3471A0_b, 0xC3); // called from Com_Frame, seems to do renderer stuff
			utils::hook::set<uint8_t>(0x9AA9A0_b, 0xC3); // CL_CheckForResend, which tries to connect to the local server constantly
			utils::hook::set<uint8_t>(0xD2EBB0_b, 0xC3); // recommended settings check

			// sound initialization
			utils::hook::nop(0xC93213_b, 5); // snd stream thread
			utils::hook::set<uint8_t>(0xC93206_b, 0); // snd_active
			utils::hook::set<uint8_t>(0xD597C0_b, 0xC3); // init voice system

			utils::hook::nop(0xC5007B_b, 6); // unknown check in SV_ExecuteClientMessage
			utils::hook::nop(0xC4F407_b, 3); // allow first slot to be occupied
			utils::hook::nop(0x3429A7_b, 2); // properly shut down dedicated servers
			utils::hook::nop(0x34296F_b, 2); // ^
			utils::hook::set<uint8_t>(0xE08360_b, 0xC3); // don't shutdown renderer

			utils::hook::set<uint8_t>(0xC5A200_b, 0xC3); // disable host migration

			// iw7 patches
			utils::hook::set(0xE06060_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE06060_b, 0xC3); // directx
			utils::hook::set(0xE05B80_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE05B80_b, 0xC3); // ^
			utils::hook::set(0xDD2760_b, 0xC3C033); //utils::hook::set<uint8_t>(0xDD2760_b, 0xC3); // ^
			utils::hook::set(0xE05E20_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE05E20_b, 0xC3); // ^ buffer
			utils::hook::set(0xE11270_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE11270_b, 0xC3); // ^
			utils::hook::set(0xDD3C50_b, 0xC3C033); //utils::hook::set<uint8_t>(0xDD3C50_b, 0xC3); // ^
			utils::hook::set(0x0C1210_b, 0xC3C033); //utils::hook::set<uint8_t>(0x0C1210_b, 0xC3); // ^ idk
			utils::hook::set(0x0C12B0_b, 0xC3C033); //utils::hook::set<uint8_t>(0x0C12B0_b, 0xC3); // ^ idk
			utils::hook::set(0xE423A0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE423A0_b, 0xC3); // directx
			utils::hook::set(0xE04680_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE04680_b, 0xC3); // ^

			utils::hook::set(0xE00ED0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE00ED0_b, 0xC3); // Image_Create1DTexture_PC
			utils::hook::set(0xE00FC0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE00FC0_b, 0xC3); // Image_Create2DTexture_PC
			utils::hook::set(0xE011A0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE011A0_b, 0xC3); // Image_Create3DTexture_PC
			utils::hook::set(0xE015C0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE015C0_b, 0xC3); // Image_CreateCubeTexture_PC
			utils::hook::set(0xE01300_b, 0xC3C033); //utils::hook::set<uint8_t>(0xE01300_b, 0xC3); // Image_CreateArrayTexture_PC

			utils::hook::set(0x5F1EA0_b, 0xC3C033); //utils::hook::set<uint8_t>(0x5F1EA0_b, 0xC3); // renderer
			utils::hook::set(0x0C1370_b, 0xC3C033); //utils::hook::set<uint8_t>(0x0C1370_b, 0xC3); // ^
			utils::hook::set(0xDD26E0_b, 0xC3C033); //utils::hook::set<uint8_t>(0xDD26E0_b, 0xC3); // directx
			utils::hook::set(0x5F0610_b, 0xC3C033); //utils::hook::set<uint8_t>(0x5F0610_b, 0xC3); // ^
			utils::hook::set(0x5F0580_b, 0xC3C033); //utils::hook::set<uint8_t>(0x5F0580_b, 0xC3); // ^
			utils::hook::set(0x5F0820_b, 0xC3C033); //utils::hook::set<uint8_t>(0x5F0820_b, 0xC3); // ^
			utils::hook::set(0x5F0790_b, 0xC3C033); //utils::hook::set<uint8_t>(0x5F0790_b, 0xC3); // ^

			utils::hook::set(0xDD42A0_b, 0xC3C033); // shutdown
			utils::hook::set(0xDD42E0_b, 0xC3C033); // ^
			utils::hook::set(0xDD42E0_b, 0xC3C033); // ^
			utils::hook::set(0xDD4280_b, 0xC3C033); // ^

			utils::hook::set(0xDD4230_b, 0xC3C033); // ^

			// r_loadForRenderer
			//utils::hook::set<uint8_t>(0xE114A0_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xE11380_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xE113D0_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xE476F0_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xE11420_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xDD2300_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xDD2610_b, 0xC3); // ^
			//utils::hook::set<uint8_t>(0xE11F40_b, 0xC3); // ^

			// skip R_GetFrameIndex check in DB_LoadLevelXAssets
			utils::hook::set<uint8_t>(0x3B9E72_b, 0xEB);

			// don't release buffer
			utils::hook::set<uint8_t>(0xDD4430_b, 0xC3);

			// R_LoadWorld
			utils::hook::set<uint8_t>(0xDD14C0_b, 0xC3);

			// something to do with vls?
			utils::hook::set<uint8_t>(0xD02CB0_b, 0xC3);

			// image stream (pak)
			utils::hook::set<uint8_t>(0xA7DB10_b, 0xC3); // DB_CreateGfxImageStreamInternal

			// set game mode
			scheduler::once([]()
			{
				if (utils::flags::has_flag("cpMode") || utils::flags::has_flag("zombies"))
				{
					game::Com_GameMode_SetDesiredGameMode(game::GAME_MODE_CP);
				}
				else
				{
					game::Com_GameMode_SetDesiredGameMode(game::GAME_MODE_MP);
				}
			}, scheduler::pipeline::main);

			// initialization
			scheduler::on_game_initialized([]()
			{
				initialize();

				console::info("==================================\n");
				console::info("Server started!\n");
				console::info("==================================\n");

				// remove disconnect command
				game::Cmd_RemoveCommand("disconnect");

				execute_startup_command_queue();
			}, scheduler::pipeline::main, 1s);

			// dedicated info
			scheduler::loop([]()
			{
				auto* sv_running = game::Dvar_FindVar("sv_running");
				if (!sv_running || !sv_running->current.enabled)
				{
					console::set_title("IW7-Mod Dedicated Server");
					return;
				}

				auto* const sv_hostname = game::Dvar_FindVar("sv_hostname");
				auto* const mapname = game::Dvar_FindVar("mapname");

				if (!sv_hostname || !mapname)
				{
					return;
				}

				std::string cleaned_hostname;
				cleaned_hostname.resize(static_cast<int>(strlen(sv_hostname->current.string) + 1));

				utils::string::strip(sv_hostname->current.string, cleaned_hostname.data(),
					static_cast<int>(strlen(sv_hostname->current.string)) + 1);

				console::set_title(utils::string::va("%s on %s", cleaned_hostname.data(), mapname->current.string));
			}, scheduler::pipeline::main, 1s);
		}
	};
}

REGISTER_COMPONENT(dedicated::component)