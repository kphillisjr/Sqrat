//
// SqratUtil: Squirrel Utilities
//

//
// Copyright (c) 2009 Brandon Jones
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//  claim that you wrote the original software. If you use this software
//  in a product, an acknowledgment in the product documentation would be
//  appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be
//  misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source
//  distribution.
//

#if !defined(_SCRAT_UTIL_H_)
#define _SCRAT_UTIL_H_

#include <cassert>
#include <squirrel.h>
#include <string.h>

namespace Sqrat {

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @cond DEV
/// removes unused variable warnings in a way that Doxygen can understand
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void UNUSED(const T&) {
}
/// @endcond

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @cond DEV
/// used internally to get the underlying type of variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> struct remove_const          {typedef T type;};
template<class T> struct remove_const<const T> {typedef T type;};
template<class T> struct remove_const<const T*> {typedef T* type;};
template<class T> struct remove_reference      {typedef T type;};
template<class T> struct remove_reference<T&>  {typedef T type;};
/// @endcond

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Defines a string that is definitely compatible with the version of Squirrel being used (normally this is std::string)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::basic_string<SQChar> string;

/// @cond DEV
#ifdef SQUNICODE
/* from http://stackoverflow.com/questions/15333259/c-stdwstring-to-stdstring-quick-and-dirty-conversion-for-use-as-key-in,
   only works for ASCII chars */
/**
* Convert a std::string into a std::wstring
*/
static std::wstring ascii_string_to_wstring(const std::string& str)
{
    return std::wstring(str.begin(), str.end());
}

/**
* Convert a std::wstring into a std::string
*/
static std::string ascii_wstring_to_string(const std::wstring& wstr)
{
    return std::string(wstr.begin(), wstr.end());
}

static std::wstring (*string_to_wstring)(const std::string& str) = ascii_string_to_wstring;
static std::string (*wstring_to_string)(const std::wstring& wstr) = ascii_wstring_to_string;
#endif // SQUNICODE
/// @endcond

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper class that defines a VM that can be used as a fallback VM in case no other one is given to a piece of code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DefaultVM {
private:
    static HSQUIRRELVM& staticVm() {
        static HSQUIRRELVM vm;
        return vm;
    }
public:
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Gets the default VM
    ///
    /// \return Default VM
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static HSQUIRRELVM Get() {
        return staticVm();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Sets the default VM to a given VM
    ///
    /// \param vm New default VM
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static void Set(HSQUIRRELVM vm) {
        staticVm() = vm;
    }
};

#if !defined (SCRAT_NO_ERROR_CHECKING)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// The class that must be used to deal with errors that Sqrat has
///
/// \remarks
/// When documentation in Sqrat says, "This function MUST have its error handled if it occurred," that
/// means that after the function has been run, you must call Error::Occurred to see if the function
/// ran successfully. If the function did not run successfully, then you must either call Error::Clear
/// or Error::Message to clear the error buffer so new ones may occur and Sqrat does not get confused.
///
/// \remarks
/// Any error thrown inside of a bound C++ function will be also thrown in the given Squirrel VM.
///
/// \remarks
/// If compiling with SCRAT_NO_ERROR_CHECKING defined, Sqrat will run significantly faster,
/// but it will no longer check for errors and the Error class itself will not be defined.
/// In this mode, a Squirrel script may crash the C++ application if errors occur in it.
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Error {
public:
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Gets the Error instance (this class uses a singleton pattern)
    ///
    /// \return Error instance
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static Error& Instance() {
        static Error instance;
        return instance;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Returns a string that has been formatted to give a nice type error message (for usage with Class::SquirrelFunc)
    ///
    /// \param vm           VM the error occurred with
    /// \param idx          Index on the stack of the argument that had a type error
    /// \param expectedType The name of the type that the argument should have been
    ///
    /// \return String containing a nice type error message
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static string FormatTypeError(HSQUIRRELVM vm, SQInteger idx, const string& expectedType) {
        string err = _SC("wrong type (") + expectedType + _SC(" expected");
        if (SQ_SUCCEEDED(sq_typeof(vm, idx))) {
            const SQChar* actualType;
            sq_tostring(vm, -1);
            sq_getstring(vm, -1, &actualType);
            sq_pop(vm, 1);
            err = err + _SC(", got ") + actualType + _SC(")");
        } else {
            err = err + _SC(", got unknown)");
        }
        sq_pop(vm, 1);
        return err;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Clears the error associated with a given VM
    ///
    /// \param vm Target VM
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void Clear(HSQUIRRELVM vm) {
        //TODO: use mutex to lock errMap in multithreaded environment
        errMap.erase(vm);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Clears the error associated with a given VM and returns the associated error message
    ///
    /// \param vm Target VM
    ///
    /// \return String containing a nice error message
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    string Message(HSQUIRRELVM vm) {
        //TODO: use mutex to lock errMap in multithreaded environment
        string err = errMap[vm];
        errMap.erase(vm);
        return err;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Returns whether a Sqrat error has occurred with a given VM
    ///
    /// \param vm Target VM
    ///
    /// \return True if an error has occurred, otherwise false
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool Occurred(HSQUIRRELVM vm) {
        //TODO: use mutex to lock errMap in multithreaded environment
        return errMap.find(vm) != errMap.end();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Raises an error in a given VM with a given error message
    ///
    /// \param vm  Target VM
    /// \param err A nice error message
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void Throw(HSQUIRRELVM vm, const string& err) {
        //TODO: use mutex to lock errMap in multithreaded environment
        if (errMap.find(vm) == errMap.end()) {
            errMap[vm] = err;
        }
    }

private:
    Error() {}

    std::map< HSQUIRRELVM, string > errMap;
};
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Tells Sqrat whether Squirrel error handling should be used
///
/// \remarks
/// If true, if a runtime error occurs during the execution of a call, the VM will invoke its error handler.
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ErrorHandling {
private:
    static bool& errorHandling() {
        static bool eh = true;
        return eh;
    }
public:
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Returns whether Squirrel error handling is enabled
    ///
    /// \return True if error handling is enabled, otherwise false
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static bool IsEnabled() {
        return errorHandling();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Enables or disables Squirrel error handling
    ///
    /// \param enable True to enable, false to disable
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static void Enable(bool enable) {
        errorHandling() = enable;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Returns the last error that occurred with a Squirrel VM (not associated with Sqrat errors)
///
/// \param vm Target VM
///
/// \return String containing a nice error message
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline string LastErrorString(HSQUIRRELVM vm) {
    const SQChar* sqErr;
    sq_getlasterror(vm);
    if (sq_gettype(vm, -1) == OT_NULL) {
		sq_pop(vm, 1);
        return string();
    }
    sq_tostring(vm, -1);
    sq_getstring(vm, -1, &sqErr);
    sq_pop(vm, 2);
    return string(sqErr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// A smart pointer that retains shared ownership of an object through a pointer (see std::shared_ptr)
///
/// \tparam T Type of pointer
///
/// \remarks
/// SharedPtr exists to automatically delete an object when all references to it are destroyed.
///
/// \remarks
/// std::shared_ptr was not used because it is a C++11 feature.
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
class SharedPtr
{
private:
    T*            m_Ptr;
    unsigned int* m_RefCount;
public:
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Constructs a new SharedPtr
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SharedPtr() :
    m_Ptr     (NULL),
    m_RefCount(NULL)
    {

    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Constructs a new SharedPtr from an object allocated with the new operator
    ///
    /// \param ptr Should be the return value from a call to the new operator
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SharedPtr(T* ptr)
    {
        Init(ptr);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Constructs a new SharedPtr from an object allocated with the new operator
    ///
    /// \param ptr Should be the return value from a call to the new operator
    ///
    /// \tparam U Type of pointer (usually doesnt need to be defined explicitly)
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <class U>
    SharedPtr(U* ptr)
    {
        Init(ptr);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Copy constructor
    ///
    /// \param copy SharedPtr to copy
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SharedPtr(const SharedPtr<T>& copy)
    {
        if (copy.Get() != NULL)
        {
            m_Ptr = copy.Get();

            m_RefCount = copy.m_RefCount;
            *m_RefCount += 1;
        }
        else
        {
            m_Ptr      = NULL;
            m_RefCount = NULL;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Copy constructor
    ///
    /// \param copy SharedPtr to copy
    ///
    /// \tparam U Type of copy (usually doesnt need to be defined explicitly)
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <class U>
    SharedPtr(const SharedPtr<U>& copy)
    {
        if (copy.Get() != NULL)
        {
            m_Ptr = static_cast<T*>(copy.Get());

            m_RefCount = copy.m_RefCount;
            *m_RefCount += 1;
        }
        else
        {
            m_Ptr      = NULL;
            m_RefCount = NULL;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Destructs the owned object if no more SharedPtr link to it
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ~SharedPtr()
    {
        Reset();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Assigns the SharedPtr
    ///
    /// \param copy SharedPtr to copy
    ///
    /// \return The SharedPtr itself
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    SharedPtr<T>& operator=(const SharedPtr<T>& copy)
    {
        if (this != &copy)
        {
            Reset();

            if (copy.Get() != NULL)
            {
                m_Ptr = copy.Get();

                m_RefCount = copy.m_RefCount;
                *m_RefCount += 1;
            }
            else
            {
                m_Ptr      = NULL;
                m_RefCount = NULL;
            }
        }

        return *this;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Assigns the SharedPtr
    ///
    /// \param copy SharedPtr to copy
    ///
    /// \tparam U Type of copy (usually doesnt need to be defined explicitly)
    ///
    /// \return The SharedPtr itself
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <class U>
    SharedPtr<T>& operator=(const SharedPtr<U>& copy)
    {
        Reset();

        if (copy.Get() != NULL)
        {
            m_Ptr = static_cast<T*>(copy.Get());

            m_RefCount = copy.m_RefCount;
            *m_RefCount += 1;
        }
        else
        {
            m_Ptr      = NULL;
            m_RefCount = NULL;
        }

        return *this;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Sets up a new object to be managed by the SharedPtr
    ///
    /// \param ptr Should be the return value from a call to the new operator
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void Init(T* ptr)
    {
        Reset();

        m_Ptr = ptr;

        m_RefCount = new unsigned int;
        *m_RefCount = 1;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Sets up a new object to be managed by the SharedPtr
    ///
    /// \param ptr Should be the return value from a call to the new operator
    ///
    /// \tparam U Type of copy (usually doesnt need to be defined explicitly)
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <class U>
    void Init(U* ptr)
    {
        Reset();

        m_Ptr = static_cast<T*>(ptr);

        m_RefCount = new unsigned int;
        *m_RefCount = 1;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Clears the owned object for this SharedPtr and deletes it if no more SharedPtr link to it
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    void Reset()
    {
        if (m_Ptr != NULL)
        {
            if (*m_RefCount == 1)
            {
                delete m_Ptr;
                delete m_RefCount;

                m_Ptr      = NULL;
                m_RefCount = NULL;
            }
            else
                *m_RefCount -= 1;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Checks if there is an associated managed object
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    operator bool() const
    {
        return m_Ptr != NULL;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Checks if there is NOT an associated managed object
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool operator!() const
    {
        return m_Ptr == NULL;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another SharedPtr
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool operator ==(const SharedPtr<U>& right) const
    {
        return m_Ptr == right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another SharedPtr
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool operator ==(const SharedPtr<T>& right) const
    {
        return m_Ptr == right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool friend operator ==(const SharedPtr<T>& left, const U* right)
    {
        return left.m_Ptr == right;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool friend operator ==(const SharedPtr<T>& left, const T* right)
    {
        return left.m_Ptr == right;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool friend operator ==(const U* left, const SharedPtr<T>& right)
    {
        return left == right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool friend operator ==(const T* left, const SharedPtr<T>& right)
    {
        return left == right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another SharedPtr
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool operator !=(const SharedPtr<U>& right) const
    {
        return m_Ptr != right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another SharedPtr
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool operator !=(const SharedPtr<T>& right) const
    {
        return m_Ptr != right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool friend operator !=(const SharedPtr<T>& left, const U* right)
    {
        return left.m_Ptr != right;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool friend operator !=(const SharedPtr<T>& left, const T* right)
    {
        return left.m_Ptr != right;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    template <typename U>
    bool friend operator !=(const U* left, const SharedPtr<T>& right)
    {
        return left != right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Compares with another pointer
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    bool friend operator !=(const T* left, const SharedPtr<T>& right)
    {
        return left != right.m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Dereferences pointer to the managed object
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    T& operator*() const
    {
        assert(m_Ptr != NULL);
        return *m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Dereferences pointer to the managed object
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    T* operator->() const
    {
        assert(m_Ptr != NULL);
        return m_Ptr;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Gets the underlying pointer
    ///
    /// \return Pointer to the managed object
    ///
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    T* Get() const
    {
        return m_Ptr;
    }
};

}

#endif
