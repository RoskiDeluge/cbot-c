## Cbot-cli Basics

The application is a C script that prompts the Llama/Ollama text completion endpoint with a system message and can identify the OS of the current system. This helps ensure that Linux, Mac, and Windows specific commands tend to be more accurate.

This project is a port of the original Cbot-cli python project into pure C. 

## Building

To build the project, you will need to have `gcc`, `make`, `libcurl`, `sqlite3`, and `jansson` installed. Then, you can run the following command:

```
make
```

This will create the `cbot` executable in the `dist` directory.

## Usage

```
./dist/cbot [options] <question>
```

### Options

*   `-l 32`: Use the Llama 3.2 model.
*   `-d`: Use the DeepSeek-R1 model.
*   `-o a`: Use the OpenAI O4-Mini model.
*   `-a`: Enter agent mode for conversational interaction.
*   `-x`: Execute the generated command.
*   `-c`: Copy the generated command to the clipboard.
*   `-g`: General question mode (not command-line specific).
*   `-s <name> <command>`: Save a command as a shortcut.
*   `-m`: Show conversation history.
*   `-h`: Display help.