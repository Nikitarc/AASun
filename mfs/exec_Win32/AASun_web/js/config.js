
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

const ENERGY_COUNT      = 9 ;           // Count of energies data to display

//------------------------------------------------------------------
// To conditionnaly show/hide some items on the 1st display update

const hideCT3 = document.querySelectorAll (".hide3") ;
const hideCT4 = document.querySelectorAll (".hide4") ;

const showMode = '' ;
const hideMode = 'none' ;
let firstShow = true ;

let hasCT3 = false ;
let hasCT4 = false ;

function setHideShow (list, display)
{
    console.log ("setHideShow" + display) ;
    for (let i = 0 ; i < list.length ; i++)
    {
        list[i].style.display = display ;
    }
}

//------------------------------------------------------------------
//------------------------------------------------------------------
//  IPV4 address utilities

// Check if the string is a valid IP
// https://melvingeorge.me/blog/check-if-string-is-valid-ip-address-javascript
function isValidIP (str)
{
    // Regular expression to check if string is a IP address
    const regexExp = /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/gi;
  
    return regexExp.test(str);
}

// Convert IP string to a 32 bits number
// http://www.java2s.com/ref/javascript/javascript-number-integer-ip-address-convert-to-integer.html
function ipToInt32 (ipString) 
{
    return ipString.split(".").reduce((int, v) => int * 256 + +v);
}

// Convert a 32 bits number to IPV4 adress string
function int32ToIp (ipInt)
{
    // Notice the >>> !
    return ( (ipInt>>>24) + '.' + (ipInt>>16 & 255) + '.' + (ipInt>>8 & 255) + '.' + (ipInt & 255) );
}

/*
console.log ("ip 192.168.1.130 " + isValidIP ("192.168.1.130")) ;
console.log ("ip 192 168.1.130 " + isValidIP ("192 168.1.130")) ;
console.log ("ip 192.168.1.130 is " + ipToInt32 ("192.168.1.130")) ;

console.log ("int32ToIp " + ipToInt32 ("192.168.1.130") + " = " + int32ToIp (ipToInt32 ("192.168.1.130"))) ;
*/

//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
// https://dmitripavlutin.com/javascript-fetch-async-await/

// Send data to the server
// Returns null or an array of strings (from the server answer):
//      Success :  "OK mid"
//      Fail:      "Error mid number" 
// If the server isn't reachabe: displays a dialog signaling the error and returns null

async function sendToServer (urlData, jsonData, bLedOn)
{
console.log ("sendToServer " + urlData + " " + JSON.stringify (jsonData)) ;

    // if the message to the server will updates the configuration, bLedOn must be true
    if (bLedOn == true)
    {
        // Put on the write red LED
        common.writeLedOn () ;
    }
    const response = await fetch (urlData, { method: "POST",
                        headers: { "Content-type": "application/json" },
                        body: JSON.stringify (jsonData) }) ;
    if (! response.ok)
    {
        common.ackDialog ("Error", "Status: " + response.status) ;
        return null ;
    }
    const answer = await response.text() ;
    return answer.split (" ") ;     // An array of string which 1st item is 'OK' or 'Error'
}

//------------------------------------------------------------------
//  Get data from the server
//  On fail returns null
//  On success returns a json object with the server data

async function getServerData (urldata)
{
console.log ("getServerData " + urldata) ;
    const response = await fetch(urldata, { method: "GET", headers: {"Content-Type": "application/json" } }) ;

    if (! response.ok)
    {
        common.ackDialog ("Error", "Status: " + response.status) ;
        return null ;
    }
    const answer = await response.json() ;
    return answer ;     // answer is a promise waiting for a JSON object
}

//------------------------------------------------------------------
//------------------------------------------------------------------

const sendVoltButton = document.getElementById("sendVolt") ;
sendVoltButton.addEventListener("click", sendVoltButtonClick);

function sendVoltButtonClick()
{
console.log ("sendVolt click") ;
    let vCal     = document.getElementById("vCal").value ;
    let phaseCal = document.getElementById("phaseCal").value ;
    if (vCal <= 0 || phaseCal <= 0)
    {
        common.ackDialog ("Volt", "Bad value") ;
    }
    else
    {
        let string = "{" +
        '"mid":"'      + common.midVolt  + '",' +
        '"vCal":"'     + vCal            + '",' +
        '"phaseCal":"' + phaseCal        + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
                if (array !== null)
                {
                    if (array [0] != "OK")
                    {
                        common.ackDialog ("Server error: ", array [2]) ;
                    }
                }
            })
    }
}

//------------------------------------------------------------------

const sendCurrentButton = document.getElementById("sendCurrent") ;
sendCurrentButton.addEventListener("click", sendCurrentButtonClick);

function sendCurrentButtonClick()
{
    let Ok = true ;
    let ct ;

console.log ("sendCurrent click") ;
    let i1Cal    = document.getElementById("i1Cal").value ;
    let i1Offset = document.getElementById("i1Offset").value ;
    let i2Cal    = document.getElementById("i2Cal").value ;
    let i2Offset = document.getElementById("i2Offset").value ;
    let i3Cal ;
    let i3Offset ;
    let i4Cal ;
    let i4Offset ;

    if (i1Cal <= 0 ||  i1Offset.length == 0)
    {
        Ok = false ;
        ct = 1 ;
    }
    if (i2Cal <= 0  ||  i2Offset.length == 0)
    {
        Ok = false ;
        ct = 2 ;
    }
    if (hasCT3)
    {
        i3Cal    = document.getElementById("i3Cal").value ;
        i3Offset = document.getElementById("i3Offset").value ;
        if (i3Cal <= 0  ||  i3Offset.length == 0)
        {
            Ok = false ;
            ct = 3 ;
        }
    }
    if (hasCT4)
    {
        i4Cal    = document.getElementById("i4Cal").value ;
        i4Offset = document.getElementById("i4Offset").value ;
        if (i4Cal <= 0  ||  i4Offset.length == 0)
        {
            Ok = false ;
            ct = 4 ;
        }
    }

    if (Ok)
    {
        let string =
            '{"mid":"'      + common.midCurrent + '"' +
            ',"i1Cal":"'    + i1Cal             + '"' +
            ',"i1Offset":"' + i1Offset          + '"' +
            ',"i2Cal":"'    + i2Cal             + '"' +
            ',"i2Offset":"' + i2Offset          + '"' ;

        if (hasCT3)
        {
            string +=
            ',"i3Cal":"'   + i3Cal              + '"' +
            ',"i3Offset":"' + i3Offset          + '"' ;
        }
        if (hasCT4)
        {
            string +=
            ',"i4Cal":"'   + i4Cal              + '"' +
            ',"i4Offset":"' + i4Offset          + '"' ;
        }
        string += '}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
                if (array !== null)
                {
                    if (array [0] != "OK")
                    {
                        common.ackDialog ("Server error: ", array [2]) ;
                    }
                }
            })
    }
    else
    {
        common.ackDialog ("Current " + ct, "Bad value") ;
    }
}

//------------------------------------------------------------------

const sendPowerButton = document.getElementById("sendPower") ;
sendPowerButton.addEventListener("click", sendPowerButtonClick);

function sendPowerButtonClick()
{
    let Ok = true ;
    let ct ;

console.log ("sendPower click") ;
    let p1Cal     = document.getElementById("p1Cal").value ;
    let p1Offset  = document.getElementById("p1Offset").value ;
    let p2Cal     = document.getElementById("p2Cal").value ;
    let p2Offset  = document.getElementById("p2Offset").value ;
    let p3Cal ;
    let p3Offset ;
    let p4Cal ;
    let p4Offset ;

    if (p1Cal <= 0  ||  p1Offset.length == 0)
    {
        Ok = false ;
        ct = 1 ;
    }
    if (p2Cal <= 0  ||  p2Offset.length == 0)
    {
        Ok = false ;
        ct = 2 ;
    }
    if (hasCT3)
    {
        p3Cal    = document.getElementById("p3Cal").value ;
        p3Offset = document.getElementById("p3Offset").value ;
        if (p3Cal <= 0  ||  p3Offset.length == 0)
        {
            Ok = false ;
            ct = 3 ;
        }
    }
    if (hasCT4)
    {
        p4Cal    = document.getElementById("p4Cal").value ;
        p4Offset = document.getElementById("p4Offset").value ;
        if (p4Cal <= 0  ||  p4Offset.length == 0)
        {
            Ok = false ;
            ct = 4 ;
        }
    }

    if (Ok)
    {
        let string =
            '{"mid":"'      + common.midPower + '"' +
            ',"p1Cal":"'    + p1Cal           + '"' +
            ',"p1Offset":"' + p1Offset        + '"' +
            ',"p2Cal":"'    + p2Cal           + '"' +
            ',"p2Offset":"' + p2Offset        + '"' ;
        if (hasCT3)
        {
            string +=
            ',"p3Cal":"'    + p3Cal           + '"' +
            ',"p3Offset":"' + p3Offset        + '"' ;
        }
        if (hasCT4)
        {
            string +=
            ',"p4Cal":"'    + p4Cal           + '"' +
            ',"p4Offset":"' + p4Offset        + '"' ;
        }
        string += '}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
                if (array !== null)
                {
                    if (array [0] != "OK")
                    {
                        common.ackDialog ("Server error: ", array [2]) ;
                    }
                }
            })
    }
    else
    {
        common.ackDialog ("Power " + ct, "Bad value") ;
    }
}

//------------------------------------------------------------------

const sendEnergyNamesButton = document.getElementById("sendEnergyNames") ;
sendEnergyNamesButton.addEventListener("click", sendEnergyNamesButtonClick);

function sendEnergyNamesButtonClick()
{
    let string = '{"mid":"' + common.midEnergyNames + '"';
console.log ("sendEnergyNamesButtonClick click") ;

    string += ',"n0":"' + document.getElementById("eName0").value + '"' ;
    string += ',"n1":"' + document.getElementById("eName1").value + '"' ;
    string += ',"n2":"' + document.getElementById("eName2").value + '"' ;
    string += ',"n3":"' + document.getElementById("eName3").value + '"' ;
    string += ',"n4":"' + document.getElementById("eName4").value + '"' ;
    string += ',"n5":"' + document.getElementById("eName5").value + '"' ;
    string += ',"n6":"' + document.getElementById("eName6").value + '"' ;
    string += ',"n7":"' + document.getElementById("eName7").value + '"}' ;
console.log (string) ;
    sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Server error: ", array [2]) ;
            }
        }
    })
}

//------------------------------------------------------------------

const sendDiverterButton = document.getElementById("sendDiverter") ;
sendDiverterButton.addEventListener("click", sendDiverterButtonClick);

function sendDiverterButtonClick()
{
console.log ("sendDiverter click") ;
    let pMax1    = document.getElementById("pMax1").value ;
    let pMax2    = document.getElementById("pMax2").value ;
    let pVolt1   = document.getElementById("pVolt1").value ;
    let pVolt2   = document.getElementById("pVolt2").value ;
    let pMargin1 = document.getElementById("pMargin1").value ;
    let pMargin2 = document.getElementById("pMargin2").value ;
    // pMargin = 0 is allowed
    if (pMax1 <= 0 || pMargin1.length == 0 || pMargin1 < 0 || pVolt1.length == 0 || pVolt1 <= 0)
    {
        common.ackDialog ("Diverter 1", "Bad value") ;
    }
    else
    {
        if (pMax2 <= 0 || pMargin2.length == 0 || pMargin2 < 0 || pVolt2.length == 0 || pVolt2 <= 0)
        {
            common.ackDialog ("Diverter 2", "Bad value") ;
        }
        else
        {
            let string = "{" +
            '"mid":"'  + common.midDiverter  + '",' +
            '"pMax1":"'    + pMax1           + '",' +
            '"pVolt1":"'   + pVolt1          + '",' +
            '"pMargin1":"' + pMargin1        + '",' +
            '"pMax2":"'    + pMax2           + '",' +
            '"pVolt2":"'   + pVolt2          + '",' +
            '"pMargin2":"' + pMargin2        + '"}' ;
            sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
                if (array !== null)
                {
                    if (array [0] != "OK")
                    {
                        common.ackDialog ("Server error: ", array [2]) ;
                    }
                }
            })
        }
    }
}

//------------------------------------------------------------------

const sendCoefButton = document.getElementById("sendCoef") ;
sendCoefButton.addEventListener("click", sendCoefButtonClick);

function sendCoefButtonClick()
{
console.log ("sendCoef click") ;
    let cksP = document.getElementById("cksP").value ;
    let cksI = document.getElementById("cksI").value ;
    let ckdP = document.getElementById("ckdP").value ;
    let ckdI = document.getElementById("ckdI").value ;
    if (cksP <= 0 || cksI <= 0  ||  ckdP <= 0 || ckdI <= 0)
    {
        common.ackDialog ("PI coefficients", "Bad value") ;
    }
    else
    {
        let string = "{" +
        '"mid":"'  + common.midCoef + '",' +
        '"cksP":"' + cksP           + '",' +
        '"cksI":"' + cksI           + '",' +
        '"ckdP":"' + ckdP           + '",' +
        '"ckdI":"' + ckdI           + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
            }
        })
    }
}

//------------------------------------------------------------------

const sendCounterButton = document.getElementById("sendCounter") ;
sendCounterButton.addEventListener("click", sendCounterButtonClick);

function sendCounterButtonClick()
{
console.log ("sendCounter click") ;
    let c1 = document.getElementById("counter1").value ;
    let c2 = document.getElementById("counter2").value ;
    if (c1 < 0 || c2 < 0)
    {
        common.ackDialog ("Counter", "Bad value") ;
    }
    else
    {
        let string = "{" +
        '"mid":"'      + common.midCounter  + '",' +
        '"counter1":"' + c1                 + '",' +
        '"counter2":"' + c2                 + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
            }
        })
    }
}

//------------------------------------------------------------------

const sendLanButton = document.getElementById("sendLan") ;
sendLanButton.addEventListener("click", sendLanButtonClick);

function sendLanButtonClick()
{
console.log ("sendLan click") ;
    let clip   = document.getElementById("clip").value ;
    let clmask = document.getElementById("clmask").value ;
    let clgw   = document.getElementById("clgw").value ;
    let cldns  = document.getElementById("cldns").value ;
    if (! isValidIP (clip) || ! isValidIP (clmask)  ||  ! isValidIP (clgw)  ||  ! isValidIP (cldns))
    {
        common.ackDialog ("Lan", "Bad value") ;
    }
    else
    {
        let string = "{" +
        '"mid":"'   + common.midLan       + '",' +
        '"clip":"'  + ipToInt32 (clip)    + '",' +
        '"clmask":"'+ ipToInt32 (clmask)  + '",' +
        '"clgw":"'  + ipToInt32 (clgw)    + '",' +
        '"cldns":"' + ipToInt32 (cldns)   + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
            }
        })
    }
}

//------------------------------------------------------------------

const sendFpButton = document.getElementById("sendFp") ;
sendFpButton.addEventListener("click", sendFpButtonClick);

function sendFpButtonClick()
{
console.log ("sendFp click") ;
    let cfp = document.getElementById("cfp").value ;
    if (cfp.length == 0  ||  cfp < 0)
    {
        common.ackDialog ("Favorite page", "Bad value") ;
    }
    else
    {
        let string = "{" +
            '"mid":"' + common.midFp  + '",' +
            '"cfp":"' + cfp           + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
            }
        })
    }
}

//------------------------------------------------------------------

const sendDisplay = document.getElementById("sendDisplay") ;
sendDisplay.addEventListener("click", sendDisplayButtonClick);

function sendDisplayButtonClick()
{
    let display = document.getElementById("displaySelect").value ;
 console.log ("sendDisplay click " + display) ;
    if (display != "0")
    {
        let string = "{" +
            '"mid":"' + common.midSetDisplay  + '",' +
            '"display":"' + display   + '"}' ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
            }
        })
    }
}

//------------------------------------------------------------------

const writeButton = document.getElementById("writeButton") ;
writeButton.addEventListener("click", writeButtonClick);

function writeButtonClick()
{
    sendToServer ("setConfig.cgi", JSON.parse ('{"mid":"' + common.midWrite + '"}'), false).then (array => {
        if (array !== null)
        {
            if (array [0] == "OK")
            {
                // Put off the write red LED
                common.writeLedOff () ;
            }
            else
            {
                common.ackDialog ("Error", array [2]) ;
            }
        }
    }) ;
}

//------------------------------------------------------------------
//------------------------------------------------------------------
//  To get and display the configuration data

function loadConfig ()
{
    // Request the configuration data
    getServerData ("config.cgi").then (data => {
        if (data != null)
        {
console.log ("displayConfig ", JSON.stringify(data)) ;
            if (firstShow)
            {
                if (Object.hasOwn (data, "p3Cal"))
                {
                    setHideShow (hideCT3, showMode) ;
                    hasCT3 = true ;
                }
                if (Object.hasOwn (data, "p4Cal"))
                {
                    setHideShow (hideCT4, showMode) ;
                    hasCT4 = true ;
                }
                firstShow = true ;
            }
    
            document.getElementById("vCal").value      = data["vCal"] ;
            document.getElementById("phaseCal").value  = data["phaseCal"] ;
    
            document.getElementById("i1Cal").value    = data["i1Cal"] ;
            document.getElementById("i1Offset").value = data["i1Offset"] ;
            document.getElementById("i2Cal").value    = data["i2Cal"] ;
            document.getElementById("i2Offset").value = data["i2Offset"] ;
    
            document.getElementById("p1Cal").value    = data["p1Cal"] ;
            document.getElementById("p1Offset").value = data["p1Offset"] ;
            document.getElementById("p2Cal").value    = data["p2Cal"] ;
            document.getElementById("p2Offset").value = data["p2Offset"] ;
    
            if (hasCT3)
            {
                document.getElementById("i3Cal").value    = data["i3Cal"] ;
                document.getElementById("i3Offset").value = data["i3Offset"] ;
                document.getElementById("p3Cal").value    = data["p3Cal"] ;
                document.getElementById("p3Offset").value = data["p3Offset"] ;
            }
            if (hasCT4)
            {
                document.getElementById("i4Cal").value    = data["i4Cal"] ;
                document.getElementById("i4Offset").value = data["i4Offset"] ;
                document.getElementById("p4Cal").value    = data["p4Cal"] ;
                document.getElementById("p4Offset").value = data["p4Offset"] ;
            }
    
            document.getElementById("pMax1").value    = data["pMax1"] ;
            document.getElementById("pVolt1").value   = data["pVolt1"] ;
            document.getElementById("pMargin1").value = data["pMargin1"] ;
            document.getElementById("pMax2").value    = data["pMax2"] ;
            document.getElementById("pVolt2").value   = data["pVolt2"] ;
            document.getElementById("pMargin2").value = data["pMargin2"] ;
     
            document.getElementById("cksP").value     = data["cksP"] ;
            document.getElementById("cksI").value     = data["cksI"] ;
            document.getElementById("ckdP").value     = data["ckdP"] ;
            document.getElementById("ckdI").value     = data["ckdI"] ;
    
            document.getElementById("counter1").value = data["counter1"] ;
            document.getElementById("counter2").value = data["counter2"] ;
    
            document.getElementById("clip").value   = int32ToIp (Number (data["clip"])) ;
            document.getElementById("clmask").value = int32ToIp (Number (data["clmask"])) ;
            document.getElementById("clgw").value   = int32ToIp (Number (data["clgw"])) ;
            document.getElementById("cldns").value  = int32ToIp (Number (data["cldns"])) ;
    
            document.getElementById("cfp").value = data["cfp"] ;
            document.getElementById("displaySelect").value = data["display"] ;
    
            document.getElementById("eName0").value = data["n0"] ;
            document.getElementById("eName1").value = data["n1"] ;
            document.getElementById("eName2").value = data["n2"] ;
            document.getElementById("eName3").value = data["n3"] ;
            document.getElementById("eName4").value = data["n4"] ;
            document.getElementById("eName5").value = data["n5"] ;
            document.getElementById("eName6").value = data["n6"] ;
            document.getElementById("eName7").value = data["n7"] ;
            document.getElementById("eName8").value = data["n8"] ;
        }
    }) ;
}

//------------------------------------------------------------------
//  For periodic update of status from the server

function updateStatus ()
{
    // Reload diverter status data
    getServerData ("volt.cgi").then (data => {
        if (data != null)
        {
            console.log ("updateVolt ", JSON.stringify(data)) ;
            document.getElementById("vRms").innerHTML   = data["vRms"] ;
            document.getElementById("p1Real").innerHTML = data["p1Real"] ;
            document.getElementById("p1App").innerHTML  = data["p1App"] ;
            document.getElementById("p2Real").innerHTML = data["p2Real"] ;
            document.getElementById("p2App").innerHTML  = data["p2App"] ;
    
            // The current values are multiplied by 2^16 = 65536
            document.getElementById("i1Rms").innerHTML  = (Number (data["i1Rms"]) / 65536).toFixed (2) ;
            document.getElementById("i2Rms").innerHTML  = (Number (data["i2Rms"]) / 65536).toFixed (2) ;
    
            let cos = Number (data["cPhi1"]) / 1000 ; 
            document.getElementById("cPhi1").innerHTML  = cos.toFixed (3) ;
            cos = Number (data["cPhi2"]) / 1000 ; 
            document.getElementById("cPhi2").innerHTML  = cos.toFixed (3) ;
    
            if (hasCT3)
            {
                document.getElementById("i3Rms").innerHTML  = (Number (data["i3Rms"]) / 65536).toFixed (2) ;
                document.getElementById("p3Real").innerHTML = data["p3Real"] ;
                document.getElementById("p3App").innerHTML  = data["p3App"] ;
                cos = Number (data["cPhi3"]) / 1000 ; 
                document.getElementById("cPhi3").innerHTML  = cos.toFixed (3) ;
            }
            if (hasCT4)
            {
                document.getElementById("i4Rms").innerHTML  = (Number (data["i4Rms"]) / 65536).toFixed (2) ;
                document.getElementById("p4Real").innerHTML = data["p4Real"] ;
                document.getElementById("p4App").innerHTML  = data["p4App"] ;
                cos = Number (data["cPhi4"]) / 1000 ; 
                document.getElementById("cPhi4").innerHTML  = cos.toFixed (3) ;
            }
        }
    }) ;
}

//------------------------------------------------------------------

// For test only: 
// const stsButton = document.getElementById("stsButton");
// stsButton.addEventListener("click", updateStatus);

// Get and dispay configuration data
const reloadButton = document.getElementById("reloadButton");
reloadButton.addEventListener("click", loadConfig);

// '<<' button: Go back to status window
const backButton = document.getElementById("backButton") ;
backButton.addEventListener("click", () => {
        if (common.bModified)
        {
            if (! confirm ("Some data is not written to FLASH.\nDo you want to quit?"))
            {
                return ;
            }
        }
        window.location.assign ("./index.html");
    }) ;
//backButton.addEventListener("click", () => { history.back(); }) ;

//------------------------------------------------------------------
//------------------------------------------------------------------

// Default: hide CT3 and CT4
setHideShow (hideCT3, hideMode) ;
setHideShow (hideCT4, hideMode) ;

// Set footer text
document.getElementById ("footerText").innerHTML = common.getFooterString () ;

// Initialize configuration data display
loadConfig () ;

// Periodic display update of data
let updateTimer = setInterval (updateStatus, 1200) ;

//------------------------------------------------------------------
