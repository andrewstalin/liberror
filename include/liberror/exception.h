// Licensed under the MIT License <http://opensource.org/licenses/MIT>
// Author: Andrew Stalin <andrew.stalin@gmail.com>
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef _LIBERROR_EXCEPTION_H_
#define _LIBERROR_EXCEPTION_H_

#include <exception>
#include <string>

#ifdef _WIN32
#include <Windows.h>
using error_code_type = DWORD;
#elif defined (__linux__) || defined (__APPLE__)
#include <cstring>
using error_code_type = int;
#endif

#define DECLARE_ERROR_INFO(name, code, description) static constexpr liberror::ErrorInfo name = { code, description }

#define THROW_IF(condition, ex) if(condition) throw ex

namespace liberror
{
	struct ErrorInfo
	{
		const error_code_type error_code;
		const char* description;
	};

	class Exception : public std::exception
	{
	private:
		error_code_type error_code_;
		std::string description_;
		std::string context_;
		mutable std::string message_;

	public:
		explicit Exception(error_code_type error_code)
			: error_code_(error_code)
		{}

		Exception(error_code_type error_code, const char* context)
			: error_code_(error_code), context_(context)
		{}

		Exception(error_code_type error_code, const char* context, const char* description)
			: error_code_(error_code), description_(description), context_(context)
		{}

		Exception(const ErrorInfo& error_info)
			: error_code_(error_info.error_code), description_(error_info.description)
		{}

		Exception(const ErrorInfo& error_info, const char* context)
			: error_code_(error_info.error_code), description_(error_info.description), context_(context)
		{}

		Exception(const Exception&) = default;
		Exception& operator=(const Exception&) = default;

		virtual ~Exception() {}

		error_code_type error_code() const { return error_code_; }
		const std::string& context() const { return context_; }
		const std::string& description() const { return description_; }

		virtual const char* category() const = 0;

		const char* what() const noexcept override
		{
			static constexpr char HEX_MAP[] = { '0','1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

			if (message_.empty())
			{
				try
				{
					message_.reserve(description_.size() + context_.size() + 100);
					
					size_t error_code_size{ sizeof(error_code_) };
					message_.append(category()).append("[0x");

					while (error_code_size-- > 0)
					{
						auto ch = static_cast<uint8_t>(error_code_ >> (error_code_size * 8));
						message_.push_back(HEX_MAP[ch >> 4]);
						message_.push_back(HEX_MAP[ch & 0x0F]);
					}

					message_.append("]");

					if (!context_.empty())
					{
						message_.append(context_);
					}

					if (!description_.empty())
					{
						message_.append(" ").append(description_);
					}
				}
				catch(std::exception& ex)
				{
					return ex.what();
				}
			}

			return message_.c_str();
		}

	};

#ifdef _WIN32
	#define WIN32_ERROR(context) liberror::SystemException(::GetLastError(), context)

	class SystemException : public Exception
	{
	private:
		struct LocalFreeGuard
		{
			void* pointer;

			explicit LocalFreeGuard(void* p)
				: pointer(p)
			{}

			~LocalFreeGuard()
			{
				::LocalFree(pointer);
			}
		};

		static std::string GetWin32ErrorDescription(DWORD error_code)
		{
			DWORD flags{ FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER };

			//LPCVOID source{ nullptr };

			//if (error_code >= WINHTTP_ERROR_BASE)
			//{
			//	flags |= FORMAT_MESSAGE_FROM_HMODULE;
			//	source = ::GetModuleHandle("winhttp.dll");
			//}

			char* buffer{ nullptr };
			auto length = ::FormatMessageA(flags, nullptr, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), reinterpret_cast<char*>(&buffer), 0, nullptr);

			if (length == 0)
			{
				return std::string();
			}

			LocalFreeGuard _(buffer);

			if (length > 0 && buffer[length - 1] == '\n')
			{
				buffer[--length] = '\0';
			}

			if (length > 0 && buffer[length - 1] == '\r')
			{
				buffer[--length] = '\0';
			}

			return std::string(buffer);
		}

	public:
		SystemException(error_code_type error_code, const char* context)
			: Exception(error_code, context, SystemException::GetWin32ErrorDescription(error_code).c_str())
		{}

		const char* category() const override { return "WIN32"; }

	};
#elif defined (__linux__) || defined (__APPLE__)
	#define POSIX_ERROR(context) liberror::SystemException(errno, context)

	class SystemException : public Exception
	{
	public:
		SystemException(error_code_type ec, const char* cnt)
			: Exception(ec, cnt, strerror(ec))
		{}

		const char* category() const override { return "POSIX"; }
	};
#endif
}

#endif
