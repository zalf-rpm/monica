Red [
	Title:		"MONICA ZeroMQ control client"
	Author:		"Michael Berg-Mohnicke"
	Rights:		"Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)"
	License: {
		This Source Code Form is subject to the terms of the Mozilla Public
		License, v. 2.0. If a copy of the MPL was not distributed with this
		file, You can obtain one at http://mozilla.org/MPL/2.0/. 
	}
	Needs: 'View
	;Needs*: {
	;	Red >= 0.4.3
	;	%ZeroMQ-binding.red
	;}
	Based-on: "ZeroMQ request/reply client example by Kaj de Vos"
	Tabs:		4
]


#include %../../../sys-libs/red/ZeroMQ-binding/ZeroMQ-binding.red


;prin ["0MQ version:" major-version]
;	prin #"."  prin minor-version
;	prin #"."  print patch-version
;prin newline

timeout-ms: 3000
use-timeouts: false
set-timeouts: function [socket][
	set-integer socket send-timeout timeout-ms
	set-integer socket receive-timeout timeout-ms
]

defaults: [
	proxy-address "localhost"
	proxy-frontend-port 5555
	proxy-backend-port 5566
	control-address "localhost"
	control-port 8888
	publisher-control-address "localhost"
	publisher-control-port 8899
	input-address "localhost"
	input-port 6666
	output-address "localhost"
	output-port 7777
]

;address: any [get-argument 1 tcp://localhost:8888]
stop-msg-template: {{"type": "finish"}}
stop-pub-msg-template: {finish{"type": "finish"}}

control-stop-msg-template: {
	{	"type": "finish"
	,	"count": <count>
	}
}

service-msg-template: {
	{	"type": <type>
	,	"count": <count>
	,	"control-address": <control-address>
	,	"control-port": <control-port>
	,	"service-port": <service-port>
	}
}

proxy-msg-template: {
	{	"type": <type>
	,	"count": <count>
	,	"control-address": <control-address>
	,	"control-port": <control-port>
	,	"proxy-address": <proxy-address>
	, 	"proxy-frontend-port": <proxy-frontend-port>
	,	"proxy-backend-port": <proxy-backend-port>
	}
}

pipeline-msg-template: {
	{	"type": <type>
	,	"count": <count>
	,	"control-address": <control-address>
	,	"control-port": <control-port>
	,	"input-address": <input-address>	
	,	"input-port": <input-port>
	,	"output-address": <output-address>	
	,	"output-port": <output-port>
	}
}


log-error: does [  ; FIXME: should go to stderr
	;print ["system-error: " system-error]
	print form-error system-error
]

log?: false
log: function [out][
	if log? [print out]
]

send-msg: function [socket msg][
	log ["Sending request"]

	either send socket msg [
		log ["Sent message: " msg]
	][
		log-error
	]
]

receive-msg: function [socket /debug][
	data: ""

	log ["Receiving msg"]

	either receive/over socket data [
		log ["Received message: " data]
	][
		log-error
	]
	data
]

comment {
#system [
	i: 1
	;pi: declare pointer! [integer!]
	pi: :i
	#define h! [pointer! [integer!]]

	i!: alias struct! [d [integer!]]
	si: declare i!

	si/d: 2

	print ["i: " i " pi: " pi " pi as i!: " as i! pi " si: " si " si as h!: " as h! si] 

	h: 0
	hs: "0xFF"
	success: parse-hex hs :h
	print ["success?: " success " hs: " hs " h: " h] 

]
}

def: function ['k][
	v: select defaults k 
	either any [
		(type? v) = string! 
		v = none
	] [v] [to string! v]
]

replace-multi: function [template multi /mold*][
	t: copy template
	foreach [k v] multi [replace t k either mold* [mold v] [v]]
	t
]


connect-to: function [host port cf dcf][
	either connect socket replace-multi "tcp://host:port" reduce ["host" host "port" port] [
		cf/enable?: false
		dcf/enable?: true
	][
		log-error
		cf/enable?: false
		dcf/enable?: true
	]
]

disconnect-from: function [cf dcf][
	lep: get-string socket last-endpoint
	either all [
		not empty? lep
		disconnect socket lep
	][
		cf/enable?: true
		dcf/enable?: false
	][
		;log-error
		cf/enable?: true
		dcf/enable?: false
	]
]

stop: function [host port count][
	either zero? stopSocket: open-socket pool request! [
		log-error
	][
		if use-timeouts [set-timeouts stopSocket]

		loop count [
			address: replace-multi "tcp://host:port" reduce ["host" host "port" port ]
			
			either connect stopSocket address [
				send-msg stopSocket stop-msg-template	
				receive-msg stopSocket	
				disconnect stopSocket get-string stopSocket last-endpoint
			][
				log-error
			]
		]
	]
	unless close-socket stopSocket [log-error]
]

stop-via-pub: function [host port][
	either zero? stopSocket: open-socket pool publish! [
		log-error
	][
		if use-timeouts [set-timeouts stopSocket]

		address: replace-multi "tcp://127.0.0.1:port" reduce ["port" port]
		
		either serve stopSocket address [
			;print "now waiting to publish finish message"
			wait 2 ;wait 2 seconds for subscribers to connect
		
			send-msg stopSocket stop-pub-msg-template
					
			if not unbind stopSocket get-string stopSocket last-endpoint [log-error]
		][
			log-error
		]
	]
	unless close-socket stopSocket [log-error]
] 

either zero? pool: make-pool 1 [
	log-error
][
	either zero? socket: open-socket pool request! [
		log-error
	][
		if use-timeouts [set-timeouts socket]

		view compose/deep [
			title: "MONICA ZeroMQ control client"
			below
			
			panel 4 [
				across 
				text "Control node address:" control-address: field (def control-address)
				text "port:" control-port: field (def control-port)
				
				check "Use timeouts" data (use-timeouts) [
					if use-timeouts: face/data [set-timeouts socket]
				]
				con: button "connect" [connect-to control-address/text control-port/text con discon]
				discon: button "disconnect" disabled [disconnect-from con discon]
				text
				
				type: drop-down "start-new" data ["start-new" "start-max" "stop"] select 1 
				count: field "1" text "MONICA instances" text 

				text "Publisher control address:" pub-control-address: field (def publisher-control-address) 
				text "port:" pub-control-port: field (def publisher-control-port)   
			]
			
			tp: tab-panel [
				"Pipeline" [
					across
					text "Input address:" input-address: field (def input-address) 
					text "port:" input-port: field (def input-port) 
					return
					text "Output address:" output-address: field (def output-address) 
					text "port:" output-port: field (def output-port)
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						m: replace-multi/mold* pipeline-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-address>" pub-control-address/text
							"<control-port>" to integer! pub-control-port/data
							"<input-address>" input-address/text
							"<input-port>" to integer! input-port/data
							"<output-address>" output-address/text
							"<output-port>" to integer! output-port/data
						]
						send-msg socket m
						receive-msg socket 
					]   
					button "stop" [
						stop-via-pub pub-control-address/text to integer! pub-control-port/data
					] 
				]
				"Proxy" [
					across
					text "Proxy address:" proxy-address: field (def proxy-address) 
					text "frontend-port:" frontend-port: field (def proxy-frontend-port) 
					text "backend-port:" backend-port: field (def proxy-backend-port)
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						m: replace-multi/mold* proxy-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-address>" pub-control-address/text
							"<control-port>" to integer! pub-control-port/data
							"<proxy-address>" proxy-address/text
							"<proxy-frontend-port>" to integer! frontend-port/data
							"<proxy-backend-port>" to integer! backend-port/data
						]
						send-msg socket m
						receive-msg socket  
					]
					button "stop" [
						stop proxy-address/text to integer! frontend-port/data to integer! count/data
					] 
				]
				"Single-service" [
					across
					text "Service port:" service-port: field (def input-port)
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						m: replace-multi/mold* service-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-address>" pub-control-address/text
							"<control-port>" to integer! pub-control-port/data
							"<service-port>" to integer! service-port/data
						]
						send-msg socket m
						receive-msg socket 
					]
					button "stop" [
						stop control-address/text to integer! service-port/data 1
					] 
				]
			]
			do [
				;connect-to control-address/text control-port/text con discon
				self/actors: object [on-close: func [f e] [
					disconnect-from con discon
				]]
			]
		]
		unless close-socket socket [log-error]
	]
	unless end-pool pool [log-error]
]

