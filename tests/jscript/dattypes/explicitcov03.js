//# ==++== 
//# 
//#   
//#    Copyright (c) 2006 Microsoft Corporation.  All rights reserved.
//#   
//#    The use and distribution terms for this software are contained in the file
//#    named license.txt, which can be found in the root of this distribution.
//#    By using this software in any fashion, you are agreeing to be bound by the
//#    terms of this license.
//#   
//#    You must not remove this notice, or any other, from this software.
//#   
//# 
//# ==--== 
//####################################################################################
@cc_on


import System;

var NULL_DISPATCH = null;
var apGlobalObj;
var apPlatform;
var lFailCount;


var iTestID = 228258;

/* -------------------------------------------------------------------------
  Test: 	explicitcov03
   
  Component:	JScript
 
  Test Area:	Type Conversion
 
 ---------------------------------------------------------------------------
  
 ---------------------------------------------------------------------------
  
 
	[00]	7-Dec-2001	    qiongou: Created Test
 -------------------------------------------------------------------------*/




function explicitcov03() {

    apInitTest("explicitcov03"); 

	
	apInitScenario("1.int()");
	var i : int = int(undefined);
	if ( i!= 0) 
		apLogFailInfo ("1.",0,i, "")

	apInitScenario ("2. byte()") ;
	var b: byte = byte(undefined);
	if (b != 0)
		apLogFailInfo ("2.",0,b ,"");

	apInitScenario ("3. long() ");
	var l: long = long(undefined);
	if (l != 0)
		apLogFailInfo ("3.", 0,l ,"");
	
	apInitScenario ("4.sbyte () ")
	var sb: sbyte = sbyte(undefined);
	if (sb != 0 )
		apLogFailInfo ("4.",0, sb,"");

	apInitScenario ("5. short () ")
	var s5: short = short(undefined);
	if (s5 != 0) 
		apLogFailInfo ("5.",0,s5,"");

	apInitScenario ("6. uint()")
	var ui : uint = uint(undefined);
	if ( ui != 0) 
		apLogFailInfo("6.",0,ui,"");

	apInitScenario ("7. ulong()")
	var ul: ulong = ulong(undefined);
	if (ul != 0)
		apLogFailInfo("7.",0,ul,"");

	apInitScenario ("8. ushort()" )
	var us: ushort = ushort(undefined);
	if (us != 0)
		apLogFailInfo("7.",0,us,"");
 
	apInitScenario ("9. boolean() ");
	var b9: boolean = boolean(undefined);
	if ( b9 != false) 
		apLogFailInfo("9.",false,b9,"");
	
	apInitScenario("10. char() ");
	var c : char = char(undefined);
	if ( c != 0) 
		apLogFailInfo("10.",0,c,"");
	
	apInitScenario ("11. float() ");
	var f11: float = float(undefined);
	if ( !isNaN(f11)) 
		apLogFailInfo("11.","NaN", f11, "");

	apInitScenario("12. double() ");
	var d : double = double(undefined);
	if ( !isNaN(d)) 
		apLogFailInfo ("12.","NaN",d, "");
	
	apInitScenario("13.Number() ");
	var n : Number = Number (undefined);
	if (! isNaN(n)) 
		apLogFailInfo ("13.","NaN",n ,"");

	apInitScenario("14. deciaml()");
	var d14 : decimal = decimal(undefined);
	if (d14 != 0) 
		apLogFailInfo( "14.",0, d14, "");

//apLogFailInfo ("errorMessage, expected, actual, bugNum)
	


	apEndTest();
}




explicitcov03();


if(lFailCount >= 0) System.Environment.ExitCode = lFailCount;
else System.Environment.ExitCode = 1;

function apInitTest(stTestName) {
    lFailCount = 0;

    apGlobalObj = new Object();
    apGlobalObj.apGetPlatform = function Funca() { return "Rotor" }
    apGlobalObj.LangHost = function Funcb() { return 1033;}
    apGlobalObj.apGetLangExt = function Funcc(num) { return "EN"; }

    apPlatform = apGlobalObj.apGetPlatform();
    var sVer = "1.0";  //navigator.appVersion.toUpperCase().charAt(navigator.appVersion.toUpperCase().indexOf("MSIE")+5);
    apGlobalObj.apGetHost = function Funcp() { return "Rotor " + sVer; }
    print ("apInitTest: " + stTestName);
}

function apInitScenario(stScenarioName) {print( "\tapInitScenario: " + stScenarioName);}

function apLogFailInfo(stMessage, stExpected, stActual, stBugNum) {
    lFailCount = lFailCount + 1;
    print ("***** FAILED:");
    print ("\t\t" + stMessage);
    print ("\t\tExpected: " + stExpected);
    print ("\t\tActual: " + stActual);
}

function apGetLocale(){ return 1033; }
function apWriteDebug(s) { print("dbg ---> " + s) }
function apEndTest() {}
