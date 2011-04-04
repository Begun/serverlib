#ifndef __CLIENTSERVER_CLIENTSERVER_BASE_H
#define __CLIENTSERVER_CLIENTSERVER_BASE_H


#include <vector>

#include <boost/shared_ptr.hpp>


namespace clientserver {

// ����������: end of file ��� pipe closed. 
class eof_exception : public std::exception {

    const char* what() const throw() {
	return "end-of-file exception";
    }
};



// ������������ ������.
template <typename T>
class buffer {


public:
    boost::shared_ptr<T> m_obj;
    size_t scanned;

private:
    std::vector<unsigned char> m_buff;
    std::vector<unsigned char>::const_iterator m_cur;
    std::vector<unsigned char>::const_iterator m_end;

    // �������� ����� ����� ������ � ������.
    void fill() {
	size_t s = m_obj->recv(&(m_buff[0]), m_buff.size());

	m_cur = m_buff.begin();
	m_end = m_cur + s;
    }

public:

    // �� ��������� ���������� 64��.
    static const size_t BUFF_SIZE = 64*1024;

    buffer(boost::shared_ptr<T> o) : m_obj(o), scanned(0) {
	m_buff.resize(BUFF_SIZE);
	m_end = m_buff.end();
	m_cur = m_end;
    }

    // ������� ���� ���� �� ������.
    buffer& operator>>(unsigned char& out) {
	if (m_cur == m_end) {
	    fill();
	}

	out = *m_cur;

	++m_cur;
        ++scanned;
	return *this;
    }

    // "������������" ������. ��� ������ ���������� look-ahead.
    bool optional_read(unsigned char match) {
        if (m_cur == m_end) {
            fill();
        }

        if (*m_cur == match) {
            ++m_cur;
            ++scanned;
            return true;
        }

        return false;
    }


    // ������� ����� ����� ���� �� ������� � ������.
    buffer& operator>>(std::vector<unsigned char>& out) {

        if (out.size() == 0) return *this;

	std::vector<unsigned char>::iterator i = out.begin();
	std::vector<unsigned char>::iterator e = out.end();

	while (1) {
	    if (m_cur == m_end) {
		fill();
	    }

	    if (e-i <= m_end-m_cur) {
		std::vector<unsigned char>::const_iterator tmpe = m_cur + (e-i);

		std::copy(m_cur, tmpe, i);
		m_cur = tmpe;

                scanned += out.size();
		return *this;
	    }

	    i = std::copy(m_cur, m_end, i);
	    m_cur = m_end;
	}
    }

    // ������������ �� ������ � ����� ���!!
    // ������������ ���� � ������ ����� ������� ������� �������.
    buffer& operator<<(const std::string& s) {
        if (!s.empty ())
            m_obj->send(s.data(), s.size());
	return *this;
    }

    buffer& operator<<(const std::vector<unsigned char>& s) {
        if (!s.empty())
            m_obj->send(&(s[0]), s.size());
        return *this;
    }

    // ��������, �� ������ �����. (��������, ����� lseek.)
    void flush() {
        m_cur = m_end;
    }

    size_t bytes_scanned() const { 
        return scanned;
    }
};


// ��� ��������, �.�. � ������� ������������� buffer ������ ������������ ����� shared_ptr.

template <typename T>
inline boost::shared_ptr<buffer<T> >& operator>>(boost::shared_ptr<buffer<T> >& b, unsigned char& out) {
    *b >> out;
    return b;
}

template <typename T>
inline boost::shared_ptr<buffer<T> >& operator>>(boost::shared_ptr<buffer<T> >& b, std::vector<unsigned char>& out) {
    *b >> out;
    return b;
}

template <typename T>
inline boost::shared_ptr<buffer<T> >& operator<<(boost::shared_ptr<buffer<T> >& b, const std::string& s) {
    *b << s;
    return b;
}

template <typename T>
inline boost::shared_ptr<buffer<T> >& operator<<(boost::shared_ptr<buffer<T> >& b, const std::vector<unsigned char>& s) {
    *b << s;
    return b;
}

}


#endif
