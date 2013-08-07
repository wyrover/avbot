/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  microcai <microcai@fedoraproject.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <boost/json_create_escapes_utf8.hpp>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

#include <avhttp/detail/parsers.hpp>

#include "boost/avloop.hpp"
// #include <boost/asio/yield.hpp>

#include "avbot_rpc_server.hpp"

namespace detail{

/**
 * avbot rpc 接受的 JSON 格式为
 *
 * 	{
		"protocol":"rpc",
		"channel":"",  // 留空表示所有频道广播
		"message":{
			"text" : "text message"
		}
	}
 */
void avbot_rpc_server::process_post( std::size_t bytestransfered )
{
	pt::ptree msg;
	std::string messagebody;
	messagebody.resize( bytestransfered );
	m_streambuf->sgetn( &messagebody[0], bytestransfered );
	std::stringstream jsonpostdata( messagebody );

	try
	{
		// 读取 json
		js::read_json( jsonpostdata, msg );
	}
	catch( const js::json_parser_error & err )
	{
		// 数据不是 json 格式，视作 纯 TEXT 格式.
		msg.put( "protocol", "rpc" );
		msg.put( "channel", "" );
		msg.put( "message.text", messagebody );
	}
	catch( const pt::ptree_error &err )
	{
		// 其他错误，忽略.
	}

	try
	{
		broadcast_message( msg );
	}
	catch( const pt::ptree_error &err )
	{
		// 忽略.
	}

}

// 发送数据在这里
void avbot_rpc_server::on_pop(boost::shared_ptr< boost::asio::streambuf > v)
{
	boost::asio::async_write(*m_socket, *v,
		boost::bind<void>(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
	);
}

// 数据操作跑这里，嘻嘻.
void avbot_rpc_server::client_loop(boost::system::error_code ec, std::size_t bytestransfered)
{
	//for (;;)
	reenter(this)
	{for (;;){

		m_request.clear();
		m_streambuf = boost::make_shared<boost::asio::streambuf>();

		// 读取用户请求.
		yield avhttpd::async_read_request(
				*m_socket, *m_streambuf, m_request,
				boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
		);

		if(ec)
		{
			if (ec == avhttpd::errc::post_without_content)
			{
				yield avhttpd::async_write_response(*m_socket, avhttpd::errc::no_content,
					boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
				);
				return;
			}
			else if (ec == avhttpd::errc::header_missing_host)
			{
				yield avhttpd::async_write_response(*m_socket, avhttpd::errc::bad_request,
					boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, 0)
				);
				return;
			}
			return;
		}

		// 解析 HTTP
		if(m_request.find(avhttpd::http_options::request_method) == "GET" )
		{
			// 等待消息, 并发送.
			yield m_responses.async_pop(boost::bind(&avbot_rpc_server::on_pop, shared_from_this(), _1));
		}
		else if( m_request.find(avhttpd::http_options::request_method) == "POST")
		{
			// 这里进入 POST 处理.
			// 读取 body
			yield boost::asio::async_read(
				*m_socket, *m_streambuf,
				boost::asio::transfer_exactly(
					boost::lexical_cast<std::size_t>(m_request.find(avhttpd::http_options::content_length))
				),

				boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), _1, _2 )
			);
			// body 必须是合法有效的 JSON 格式
			process_post(bytestransfered);
		}

		// 继续
		yield avloop_idle_post(m_socket->get_io_service(),
			boost::bind(&avbot_rpc_server::client_loop, shared_from_this(), ec, 0)
		);
	}}
}

void avbot_rpc_server::callback_message(const boost::property_tree::ptree& jsonmessage)
{
	boost::shared_ptr<boost::asio::streambuf> buf(new boost::asio::streambuf);
	std::ostream	stream(buf.get());
	std::stringstream	teststream;

	js::write_json(teststream,  jsonmessage);

	// 直接写入 json 格式的消息吧!
	stream << "HTTP/1.1 200 OK\r\n" <<  "Content-type: application/json\r\n";
	stream << "connection: keep-alive\r\n" <<  "Content-length: ";
	stream << teststream.str().length() <<  "\r\n\r\n";

	js::write_json(stream, jsonmessage);

	m_responses.push(buf);
}

}
