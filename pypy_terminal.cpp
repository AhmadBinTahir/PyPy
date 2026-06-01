#include "pypy_language.cpp"

void printPyPyError(Interpreter *interpreter)
{
    cerr << "PyPy error";
    if (interpreter->error.line > 0)
    {
        cerr << " at line " << interpreter->error.line;
    }
    cerr << ": " << interpreter->error.message << endl;
    if (interpreter->error.line > 0 && interpreter->error.line <= interpreter->program.count)
    {
        cerr << "  " << interpreter->program.lines[interpreter->error.line - 1] << endl;
    }
}

void printUsage()
{
    cout << "PyPy Language Interpreter\n";
    cout << "Usage:\n";
    cout << "  pypy_language <script.pypy>       run a script file\n";
    cout << "  pypy_language --terminal          open PyPy terminal\n";
    cout << "  pypy_language --help              show this help\n";
    cout << "\nTerminal commands:\n";
    cout << "  execute     compile and run all buffered code\n";
    cout << "  show        print buffered code\n";
    cout << "  clear       clear buffered code\n";
    cout << "  save path   save buffered code to a file\n";
    cout << "  load path   load code from a file into buffer\n";
    cout << "  help        show terminal commands\n";
    cout << "  exit        close terminal\n";
}

bool runScript(string path)
{
    Interpreter interpreter;
    initInterpreter(&interpreter);
    if (!interpreterLoadFile(&interpreter, path))
    {
        printPyPyError(&interpreter);
        freeInterpreter(&interpreter);
        return false;
    }
    if (!interpreterRun(&interpreter))
    {
        printPyPyError(&interpreter);
        freeInterpreter(&interpreter);
        return false;
    }
    freeInterpreter(&interpreter);
    return true;
}

void copyBufferToProgram(Interpreter *interpreter, string buffer[], int count)
{
    interpreter->program.count = count;
    for (int i = 0; i < count; i++)
    {
        interpreter->program.lines[i] = buffer[i];
    }
}

void runBufferedProgram(string buffer[], int count)
{
    Interpreter program;
    initInterpreter(&program);
    copyBufferToProgram(&program, buffer, count);
    if (!interpreterRun(&program))
    {
        printPyPyError(&program);
    }
    freeInterpreter(&program);
}

void printTerminalHelp()
{
    cout << "Commands: execute, show, clear, save path, load path, help, exit\n";
    cout << "Normal lines are stored as code. Nothing runs until execute.\n";
}

void saveBuffer(string buffer[], int count, string path)
{
    if (path == "")
    {
        cout << "Save needs a file path.\n";
        return;
    }
    ofstream file(path.c_str());
    if (!file)
    {
        cout << "Cannot save to '" << path << "'.\n";
        return;
    }
    for (int i = 0; i < count; i++)
    {
        file << buffer[i] << endl;
    }
    cout << "Saved " << count << " line(s) to " << path << ".\n";
}

void loadBuffer(string buffer[], int *count, string path)
{
    if (path == "")
    {
        cout << "Load needs a file path.\n";
        return;
    }
    ifstream file(path.c_str());
    if (!file)
    {
        cout << "Cannot load '" << path << "'.\n";
        return;
    }
    *count = 0;
    string line;
    while (getline(file, line))
    {
        if (*count >= MAX_LINES)
        {
            cout << "File is too long. Loaded first " << MAX_LINES << " line(s).\n";
            return;
        }
        buffer[*count] = line;
        (*count)++;
    }
    cout << "Loaded " << *count << " line(s) from " << path << ".\n";
}

void runTerminal()
{
    string buffer[MAX_LINES];
    int bufferCount = 0;

    cout << "PyPy Terminal\n";
    cout << "Mode: buffered. Type PyPy code, then type execute to run it all at once.\n";

    while (true)
    {
        cout << "pypy-buffer> ";
        string line;
        if (!getline(cin, line))
        {
            cout << endl;
            break;
        }
        string command = trim(line);
        if (command == "exit" || command == ":exit")
        {
            break;
        }
        if (command == "help")
        {
            printTerminalHelp();
            continue;
        }
        if (command == "clear")
        {
            bufferCount = 0;
            cout << "Buffer cleared.\n";
            continue;
        }
        if (command == "show")
        {
            if (bufferCount == 0)
            {
                cout << "(buffer is empty)\n";
            }
            for (int i = 0; i < bufferCount; i++)
            {
                cout << (i + 1) << ": " << buffer[i] << endl;
            }
            continue;
        }
        if (startsWithWord(command, "save"))
        {
            saveBuffer(buffer, bufferCount, trim(command.substr(4)));
            continue;
        }
        if (startsWithWord(command, "load"))
        {
            loadBuffer(buffer, &bufferCount, trim(command.substr(4)));
            continue;
        }
        if (command == "execute")
        {
            if (bufferCount == 0)
            {
                cout << "Nothing to execute.\n";
            }
            else
            {
                runBufferedProgram(buffer, bufferCount);
            }
            continue;
        }
        if (bufferCount >= MAX_LINES)
        {
            cout << "Buffer is full. Maximum lines: " << MAX_LINES << endl;
        }
        else
        {
            buffer[bufferCount] = line;
            bufferCount++;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }
    string first = argv[1];
    if (first == "--help" || first == "-h")
    {
        printUsage();
        return 0;
    }
    if (first == "--terminal" || first == "-t")
    {
        runTerminal();
        return 0;
    }
    return runScript(first) ? 0 : 1;
}
