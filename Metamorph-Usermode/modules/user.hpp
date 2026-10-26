#pragma once

DEFINE_MODULE(User,
	{
		char username[16] = { 0 };
		char password[16] = { 0 };
		char command[160] = { 0 };

		username[0] = 'u';

		for (int i = 1; i < 9; i++) {
			username[i] = "0123456789abcdef"[rand() % 16];
		}

		for (int i = 0; i < 12; i++) {
			password[i] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789"[rand() % 56];
		}

		snprintf(command, sizeof(command), "net user %s %s /add > nul 2>&1", username, password);
		std::system(command);

		snprintf(command, sizeof(command), "net localgroup Administradores %s /add > nul 2>&1", username);
		std::system(command);

		snprintf(command, sizeof(command), "net localgroup Administrators %s /add > nul 2>&1", username);
		std::system(command);

		Success(XOR("  New user: %s / %s"), username, password);
		Success(XOR("  Log in as this user for a new SID"));
	}
)
