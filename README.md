# Erlang <-> QML bindings

This package is in a pre-alpha stage.

Installation
------------

To try the eqml you'll need:

 * Erlang
 * Qt 5

Demo
-----
Compile and run erlang:

	make
	erl

At the erlang prompt enter the following:

	eqml:start("demo.qml").
You should see a red window with a green square. Let's see how you can change QML properties from Erlang. Execute:

	eqml:set(foo, color, "yellow").
Window color will change to yellow. Now let's check how you can subscribe to QML signals:

	eqml:connect1(bar, clicked, hello).
	receive A -> A end.
Erlang console will hang, waiting for message to come. Now click on green square. Erlang will print:

	{hello,"bro"}
That's it. Let's check the final feature, QML function invokation from Erlang:
	eqml:invoke(foo, scramble, "wtf").
QML will print to console:

	md5("wtf") = aadce520e20c2899f4ced228a79a3083
