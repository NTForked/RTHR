#include "pch.h"
#include "TextConsole.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

TextConsole::TextConsole() : m_textColor(1.f, 1.f, 1.f, 1.f)
{
	Clear();
}

TextConsole::TextConsole(ID3D11DeviceContext* context, const wchar_t* fontName)
	: m_textColor(1.f, 1.f, 1.f, 1.f)
{
	RestoreDevice(context, fontName);

	Clear();
}

void TextConsole::Render()
{
	// Breaks if no lines were added to the console avoiding seg fault
	if (!m_lines)
		return;

	std::lock_guard<std::mutex> lock(m_mutex);

	float lineSpacing = m_font->GetLineSpacing();

	float x = float(m_layout.left);
	float y = float(m_layout.top);

	XMVECTOR color = XMLoadFloat4(&m_textColor);

	m_batch->Begin();
	unsigned int textLine = unsigned int(m_currentLine - m_rows + m_rows + 1) % m_rows;

	for (unsigned int line = 0; line < m_rows; ++line)
	{
		XMFLOAT2 pos(x, y + lineSpacing * float(line));

		if (*m_lines[textLine])
		{
			m_font->DrawString(m_batch.get(), m_lines[textLine], pos, color);
		}

		textLine = unsigned int(textLine + 1) % m_rows;
	}

	m_batch->End();
}

void TextConsole::Clear()
{
	if (m_buffer)
		memset(m_buffer.get(), 0, sizeof(wchar_t)*(m_columns + 1)*m_rows);

	m_currentColumn = m_currentLine = 0;
}

void TextConsole::Write(_In_z_ const wchar_t *str)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	ProcessString(str);
}

void TextConsole::WriteLine(const wchar_t *str)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	ProcessString(str);
	IncrementLine();
}

//void TextConsole::Format(const wchar_t* strFormat, ...)
//{
//	std::lock_guard<std::mutex> lock(m_mutex);
//
//	va_list argList;
//	va_start(argList, strFormat);
//
//	size_t len = _vscwprintf(strFormat, argList) + 1;
//
//	if (m_tempBuffer.size() < len)
//		m_tempBuffer.resize(len);
//
//	memset(m_tempBuffer.data(), 0, len);
//
//	vswprintf_s(m_tempBuffer.data(), m_tempBuffer.size(), strFormat, argList);
//
//	va_end(argList);
//
//	ProcessString(m_tempBuffer.data());
//}

void TextConsole::SetWindow(const RECT& layout)
{
	m_layout = layout;

	assert(m_font != 0);

	float lineSpacing = m_font->GetLineSpacing();
	unsigned int rows = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.bottom - layout.top) / lineSpacing));

	auto charSize = m_font->MeasureString(L"M");
	unsigned int columns = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.right - layout.left) / XMVectorGetX(charSize)));

	std::unique_ptr<wchar_t[]> buffer(new wchar_t[(columns + 1) * rows]);
	memset(buffer.get(), 0, sizeof(wchar_t) * (columns + 1) * rows);

	std::unique_ptr<wchar_t*[]> lines(new wchar_t*[rows]);
	for (unsigned int line = 0; line < rows; ++line)
	{
		lines[line] = buffer.get() + (columns + 1) * line;
	}

	if (m_lines)
	{
		unsigned int c = std::min<unsigned int>(columns, m_columns);
		unsigned int r = std::min<unsigned int>(rows, m_rows);

		for (unsigned int line = 0; line < r; ++line)
		{
			memcpy(lines[line], m_lines[line], c * sizeof(wchar_t));
		}
	}

	std::swap(columns, m_columns);
	std::swap(rows, m_rows);
	std::swap(buffer, m_buffer);
	std::swap(lines, m_lines);
}

void TextConsole::ReleaseDevice()
{
	m_batch.reset();
	m_font.reset();
	m_context.Reset();
}

void TextConsole::RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName)
{
	m_context = context;

	m_batch = std::make_unique<SpriteBatch>(context);

	ComPtr<ID3D11Device> device;
	context->GetDevice(device.GetAddressOf());

	m_font = std::make_unique<SpriteFont>(device.Get(), fontName);

	m_font->SetDefaultCharacter(L' ');
}

void TextConsole::ProcessString(const wchar_t* str)
{
	if (!m_lines)
		return;

	float width = float(m_layout.right - m_layout.left);

	for (const wchar_t* ch = str; *ch != 0; ++ch)
	{
		if (*ch == '\n')
		{
			IncrementLine();
			continue;
		}

		bool increment = false;

		if (m_currentColumn >= m_columns)
			increment = true;
		else
		{
			m_lines[m_currentLine][m_currentColumn] = *ch;

			auto fontSize = m_font->MeasureString(m_lines[m_currentLine]);
			if (XMVectorGetX(fontSize) > width)
			{
				m_lines[m_currentLine][m_currentColumn] = L'\0';

				increment = true;
			}
		}

		if (increment)
		{
			IncrementLine();
			m_lines[m_currentLine][0] = *ch;
		}

		++m_currentColumn;
	}
}

void TextConsole::IncrementLine()
{
	if (!m_lines)
		return;

	m_currentLine = (m_currentLine + 1) % m_rows;
	m_currentColumn = 0;
	memset(m_lines[m_currentLine], 0, sizeof(wchar_t)*(m_columns + 1));
}