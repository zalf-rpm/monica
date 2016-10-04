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
	control-address "tcp://localhost:8888"
	publisher-control-address "tcp://localhost:8899"
	input-address "tcp://localhost:6666"
	output-address "tcp://localhost:7777"
	service-port 6666
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
	{	"type": "<type>"
	,	"count": <count>
	,	"control-addresses": "<control-addresses>"
	,	"service-port": <service-port>
	}
}

proxy-msg-template: {
	{	"type": "<type>"
	,	"count": <count>
	,	"control-addresses": "<control-addresses>"
	,	"proxy-address": "<proxy-address>"
	, 	"proxy-frontend-port": <proxy-frontend-port>
	,	"proxy-backend-port": <proxy-backend-port>
	}
}

pipeline-msg-template: {
	{	"type": "<type>"
	,	"count": <count>
	,	"control-addresses": "<control-addresses>"
	,	"input-addresses": "<input-addresses>"	
	,	"output-addresses": "<output-addresses>"	
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
	;print k
	v: select defaults k 
	either any [
		(type? v) = string! 
		v = none
	] [v] [to string! v]
]

replace-multi: function [template multi][
	t: copy template
	foreach [k v] multi [replace t k v v]
	t
]


connect-to: function [address cf dcf][
	either connect socket address [
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

stop: function [address count][
	either zero? stopSocket: open-socket pool request! [
		log-error
	][
		if use-timeouts [set-timeouts stopSocket]

		loop count [
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

stop-via-pub: function [port][
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
						
			across 
			text "Control node address:" control-address: field 150 (def control-address)
			return
			
			check "Use timeouts" data (use-timeouts) [
				if use-timeouts: face/data [set-timeouts socket]
			]
			con: button "connect" [connect-to control-address/text con discon]
			discon: button "disconnect" disabled [disconnect-from con discon]
			return
							
			type: drop-down "start-new" data ["start-new" "start-max" "stop"] select 1 
			count: field "1" text "MONICA instances"
			return  

			text "Publisher control address(es):" pub-control-addresses: field 150 (def publisher-control-address) 
			return 

			below
			tp: tab-panel [
				"Pipeline" [
					across
					text "Input address(es):" input-addresses: field 150 (def input-address) 
					return
					text "Output address(es):" output-addresses: field 150 (def output-address) 
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						;print input-addresses/text
						m: replace-multi pipeline-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-addresses>" pub-control-addresses/text
							"<input-addresses>" input-addresses/text
							"<output-addresses>" output-addresses/text
						]
						;print m
						send-msg socket m
						receive-msg socket 
					]   
					button "stop" [
						stop-via-pub to integer! first parse pub-control-addresses/text [
							collect [some ["tcp://" thru ":" keep [to "," | thru end] opt ","]]
						] 
					] 
				]
				"Proxy" [
					across
					text "Proxy address:" proxy-address: field (def proxy-address) 
					text "frontend-port:" frontend-port: field (def proxy-frontend-port) 
					text "backend-port:" backend-port: field (def proxy-backend-port)
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						m: replace-multi proxy-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-address>" pub-control-addresses/text
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
					text "Service port:" service-port: field (def service-port)
					return
					button "action" react [face/text: type/text face/enable?: not con/enable?] [
						m: replace-multi service-msg-template reduce [
							"<type>" type/text
							"<count>" to integer! count/data
							"<control-address>" pub-control-addresses/text
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

