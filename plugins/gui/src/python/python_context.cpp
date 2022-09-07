#include "gui/python/python_context.h"

#include "gui/gui_globals.h"
#include "gui/python/python_context_subscriber.h"
#include "gui/python/python_thread.h"
#include "gui/python/python_editor.h"
#include "hal_core/python_bindings/python_bindings.h"
#include "hal_core/utilities/log.h"
#include "hal_core/utilities/utils.h"

#include <QDir>
#include <QDebug>
#include <fstream>
#include <gui/python/python_context.h>

// Following is needed for PythonContext::checkCompleteStatement
#include "hal_config.h"

#include <Python.h>
#include <compile.h>
#include <errcode.h>

#if PY_VERSION_HEX < 0x030900a0 // Python 3.9.0

#include <grammar.h>
#include <node.h>
#include <parsetok.h>
extern grammar _PyParser_Grammar;

#endif

namespace hal
{
    PythonContext::PythonContext(QObject *parent)
        : QObject(parent), mContext(nullptr), mSender(nullptr), mThread(nullptr)
    {
        py::initialize_interpreter();
        initPython();

        // The Python interpreter is not thread safe, so it implements an internal
        // Global Interpreter Lock that means only one thread can actively be
        // executing Python code at a time (though the interpreter can switch between
        // threads, for example if one thread is blocked for I/O).
        // Take care to always handle the GIL correctly, or you will cause deadlocks,
        // weird issues with threads that are running but Python believes that they
        // don't exist, etc.
        // DO NOT EVER run Python code while not holding the GIL!
        // Wherever possible, use the PyGILState_Ensure / PyGILState_Release API to
        // acquire/release the GIL.

        // We must release the GIL, otherwise any spawning thread will deadlock.
        mMainThreadState = PyEval_SaveThread();

    }

    PythonContext::~PythonContext()
    {
        PyEval_RestoreThread(mMainThreadState);

        closePython();
        py::finalize_interpreter();
    }

    void PythonContext::setConsole(PythonConsole* c)
    {
        mConsole = c;
    }

    void PythonContext::initializeContext(py::dict* context)
    {
        // GIL must be held

        std::string command = "import __main__\n"
                              "import io, sys, threading\n";
        for (auto path : utils::get_plugin_directories())
        {
            command += "sys.path.append('" + path.string() + "')\n";
        }
        command += "sys.path.append('" + utils::get_library_directory().string()
                   + "')\n"
                     "from hal_gui.console import reset\n"
                     "from hal_gui.console import clear\n"
                     "class StdOutCatcher(io.TextIOBase):\n"
                     "    def __init__(self):\n"
                     "        pass\n"
                     "    def write(self, stuff):\n"
                     "        from hal_gui import console\n"
                     "        if threading.current_thread() is threading.main_thread():\n"
                     "            console.redirector.write_stdout(stuff)\n"
                     "        else:\n"
                     "            console.redirector.thread_stdout(stuff)\n"
                     "class StdErrCatcher(io.TextIOBase):\n"
                     "    def __init__(self):\n"
                     "        pass\n"
                     "    def write(self, stuff):\n"
                     "        from hal_gui import console\n"
                     "        if threading.current_thread() is threading.main_thread():\n"
                     "            console.redirector.write_stderr(stuff)\n"
                     "        else:\n"
                     "            console.redirector.thread_stderr(stuff)\n"
                     "sys.stdout = StdOutCatcher()\n"
                     "sys.__stdout__ = sys.stdout\n"
                     "sys.stderr = StdErrCatcher()\n"
                     "sys.__stderr__ = sys.stderr\n"
                     "import hal_py\n";

        py::exec(command, *context, *context);

        (*context)["netlist"] = gNetlistOwner;    // assign the shared_ptr here, not the raw ptr

        if (gGuiApi)
        {
            (*context)["gui"] = gGuiApi;
        }
    }

    void PythonContext::initializeScript(py::dict *context)
    {
        // GIL must be held

        std::string command = "import __main__\n"
                              "import io, sys, threading\n"
                              "from hal_gui.console import reset\n"
                              "from hal_gui.console import clear\n"
                              "import hal_py\n";

        py::exec(command, *context, *context);

        (*context)["netlist"] = gNetlistOwner;    // assign the shared_ptr here, not the raw ptr
    }


    void PythonContext::initPython()
    {
        // GIL must be held

        //    using namespace py::literals;

        new py::dict();
        mContext = new py::dict(**py::globals());

        initializeContext(mContext);
        (*mContext)["console"] = py::module::import("hal_gui.console");
        (*mContext)["hal_gui"] = py::module::import("hal_gui");
    }

    void PythonContext::closePython()
    {
        delete mContext;
        mContext = nullptr;
    }

    void PythonContext::interpret(const QString& input, bool multiple_expressions)
    {
        qDebug() << "in interpret()";

        if (input.isEmpty())
        {
            return;
        }

        if (input == "quit()")
        {
            forwardError("quit() cannot be used in this interpreter. Use console.reset() to restart it.\n");
            return;
        }

        if (input == "help()")
        {
            forwardError("help() cannot be used in this interpreter.\n");
            return;
        }

        if (input == "license()")
        {
            forwardError("license() cannot be used in this interpreter.\n");
            return;
        }
        
        log_info("python", "Python console execute: \"{}\".", input.toStdString());

        qDebug() << "trying to get lock";
        
        // since we've released the GIL in the constructor, re-acquire it here before
        // running some Python code on the main thread
        PyGILState_STATE state = PyGILState_Ensure();
        //PyEval_RestoreThread(mMainThreadState);

        // TODO should the console also be moved to threads? Maybe actually catch Ctrl+C there
        // as a method to interrupt? Currently you can hang the GUI by running an endless loop
        // from the console.

        qDebug() << "have lock";

        try
        {
            pybind11::object rc;
            qDebug() << "starting execution";
            if (multiple_expressions)
            {
                rc = py::eval<py::eval_statements>(input.toStdString(), *mContext, *mContext);
            }
            else
            {
                rc = py::eval<py::eval_single_statement>(input.toStdString(), *mContext, *mContext);
            }
            qDebug() << "execution succeeded";
            if (!rc.is_none())
            {
                forwardStdout(QString::fromStdString(py::str(rc).cast<std::string>()));
            }
            handleReset();
        }
        catch (py::error_already_set& e)
        {
            qDebug() << "error set";
            forwardError(QString::fromStdString(std::string(e.what())));
            e.restore();
            PyErr_Clear();
        }
        catch (std::exception& e)
        {
            qDebug() << "exception";
            forwardError(QString::fromStdString(std::string(e.what())));
        }
        qDebug() << "trying to release lock";

        // make sure we release the GIL, otherwise we interfere with threading
        PyGILState_Release(state);
        //mMainThreadState = PyEval_SaveThread();

        qDebug() << "released lock";

    }

    void PythonContext::interpretScript(PythonEditor* editor, const QString& input)
    {
        if (mThread)
        {
            log_warning("python", "Not executed, python script already running");
            return;
        }
        // py::print(py::globals());

        //log_info("python", "Python editor execute script:\n{}\n", input.toStdString());
#ifdef HAL_STUDY
        log_info("UserStudy", "Python editor execute script:\n{}\n", input.toStdString());
#endif
        forwardStdout("\n");
        forwardStdout("<Execute Python Editor content>");
        forwardStdout("\n");

        mThread = new PythonThread(input,this);
        connect(mThread,&QThread::finished,this,&PythonContext::handleScriptFinished);
        connect(mThread,&PythonThread::stdOutput,this,&PythonContext::handleScriptOutput);
        connect(mThread,&PythonThread::stdError,this,&PythonContext::handleScriptError);
        connect(this,&PythonContext::threadFinished,editor,&PythonEditor::handleThreadFinished);
        mThread->start();
    }

    void PythonContext::handleScriptOutput(const QString& txt)
    {
        if (!txt.isEmpty())
            forwardStdout(txt);
    }

    void PythonContext::handleScriptError(const QString& txt)
    {
        if (!txt.isEmpty())
            forwardError(txt);
    }

    void PythonContext::handleScriptFinished()
    {
        if (!mThread) return;
        QString errmsg = mThread->errorMessage();
        mThread->deleteLater();
        mThread = 0;

        if (!errmsg.isEmpty())
            forwardError(errmsg);

        if (mConsole)
        {
            mConsole->displayPrompt();
        }
        qDebug() << "PythonContext::handleScriptFinished done!";
        Q_EMIT threadFinished();
    }

    void PythonContext::forwardStdout(const QString& output)
    {
        if (output != "\n")
        {
            log_info("python", "{}", utils::rtrim(output.toStdString(), "\r\n"));
        }
        if (mConsole)
        {
            mConsole->handleStdout(output);
        }
    }

    void PythonContext::forwardError(const QString& output)
    {
        log_error("python", "{}", output.toStdString());
        if (mConsole)
        {
            mConsole->handleError(output);
        }
    }

    void PythonContext::forwardClear()
    {
        if (mConsole)
        {
            mConsole->clear();
        }
    }

    std::vector<std::tuple<std::string, std::string>> PythonContext::complete(const QString& text, bool use_console_context)
    {
        PyGILState_STATE state = PyGILState_Ensure();

        std::vector<std::tuple<std::string, std::string>> ret_val;
        py::dict tmp_context;
        try
        {
            auto namespaces = py::list();
            if (use_console_context)
            {
                namespaces.append(*mContext);
                namespaces.append(*mContext);
            }
            else
            {
                tmp_context = py::globals();
                initializeContext(&tmp_context);
                namespaces.append(tmp_context);
                namespaces.append(tmp_context);
            }
            auto jedi   = py::module::import("jedi");
            py::object script = jedi.attr("Interpreter")(text.toStdString(), namespaces);
            py::object list;
            if (py::hasattr(script,"complete"))
                list = script.attr("complete")();
            else if (py::hasattr(script,"completions"))
                list   = script.attr("completions")();
            else
                log_warning("python", "Jedi autocompletion failed, neither complete() nor completions() found.");

            for (const auto& entry : list)
            {
                auto a = entry.attr("name_with_symbols").cast<std::string>();
                auto b = entry.attr("complete").cast<std::string>();
                ret_val.emplace_back(a, b);
            }
        }
        catch (py::error_already_set& e)
        {
            forwardError(QString::fromStdString(std::string(e.what()) + "\n"));
            e.restore();
            PyErr_Clear();
        }

        PyGILState_Release(state);

        return ret_val;
    }

    int PythonContext::checkCompleteStatement(const QString& text)
    {

        PyGILState_STATE state = PyGILState_Ensure();
        
        #if PY_VERSION_HEX < 0x030900a0 // Python 3.9.0
        // PEG not yet available, use PyParser

        node* n;
        perrdetail e;

        n = PyParser_ParseString(text.toStdString().c_str(), &_PyParser_Grammar, Py_file_input, &e);
        if (n == NULL)
        {
            if (e.error == E_EOF)
            {
                PyGILState_Release(state);
                return 0;
            }
            PyGILState_Release(state);
            return -1;
        }

        PyNode_Free(n);
        PyGILState_Release(state);
        return 1;

        #else

        // attempt to parse Python expression into AST using PEG
        PyCompilerFlags flags {PyCF_ONLY_AST, PY_MINOR_VERSION};
        PyObject* o = Py_CompileStringExFlags(text.toStdString().c_str(), "stdin", Py_file_input, &flags, 0);
        // if parsing failed (-> not a complete statement), nullptr is returned
        // (apparently no need to PyObject_Free(o) here)
        PyGILState_Release(state);
        return o != nullptr;

        #endif
    }

    void PythonContext::handleReset()
    {
        if (mTriggerReset)
        {
            closePython();
            initPython();
            forwardClear();
            mTriggerReset = false;
        }
    }

    void PythonContext::forwardReset()
    {
        mTriggerReset = true;
    }

    void PythonContext::updateNetlist()
    {
        (*mContext)["netlist"] = gNetlistOwner;    // assign the shared_ptr here, not the raw ptr
    }
}    // namespace hal
