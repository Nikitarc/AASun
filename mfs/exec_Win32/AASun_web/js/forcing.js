
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

//------------------------------------------------------------------
// https://stackoverflow.com/questions/3999957/what-is-a-c-c-data-structure-equivalent-in-javascript

const DIVERTING_MAX = 2 ;
const FORCING_MAX   = 8 ;
const OUT_MAX       = 4 ;

function dfItem() {
    this.display = null;
    this.led = null;
}

function divertingItem() {
    this.rule = null;
    this.off = null;
}

function forcingItem() {
    this.start = null;
    this.stop = null;
    this.off = null;
}

let diverting = [] ;
let forcing   = [] ;
let dfStatus  = [] ;
let greenLeds = [] ;

//------------------------------------------------------------------
//------------------------------------------------------------------

function getFirstWord(str) {
    let spaceIndex = str.indexOf(' ');
    return spaceIndex === -1 ? str : str.substring(0, spaceIndex);
};

//------------------------------------------------------------------
// Build diverting and forcing element arrays

greenLeds [0] = document.getElementById ("divLed1") ;
greenLeds [1] = document.getElementById ("divLed2") ;
for (let ii = 0 ; ii < FORCING_MAX ; ii++)
{
    greenLeds [DIVERTING_MAX + ii] = document.querySelector("#forceLed" + (ii+1)) ;
}

for (let ii = 1 ; ii <= OUT_MAX ; ii++)
{
    let item = new dfItem() ;
    item.display = document.getElementById ("dfStatus" + ii) ;
    item.led     = undefined ;
    dfStatus.push (item) ;
}

for (let ii = 1 ; ii <= DIVERTING_MAX ; ii++)
{
    let item = new divertingItem() ;
    item.rule = document.getElementById ("divRule" + ii) ;
    item.off  = document.getElementById ("divOff"  + ii) ;
    item.rule.setAttribute ('spellcheck', 'false') ;
    diverting.push (item) ;
}

for (let ii = 1 ; ii <= FORCING_MAX ; ii++)
{
    let item = new forcingItem() ;
    item.start = document.getElementById ("forceStart" + ii) ;
    item.stop  = document.getElementById ("forceStop"  + ii) ;
    item.start.setAttribute ('spellcheck', 'false') ;
    item.stop.setAttribute ('spellcheck', 'false') ;
    item.off = document.querySelector("#forceOff" + ii) ;
    forcing.push (item) ;
}

// https://stackoverflow.com/questions/71503820/how-to-add-an-event-listener-to-all-items-in-array

// setConfig.cgi message ID
const midWrite              = 0 ;
const midDivOnOff           = 1 ;
const midRes1               = 2 ;     // Reserved
const midRes2               = 3 ;     // Reserved
const midVolt               = 4 ;
const midCurrent            = 5 ;
const midPower              = 6 ;
const midDiverter           = 7 ;
const midCoef               = 8 ;
const midCounter            = 9 ;
const midLan                = 10 ;
const midFp                 = 11 ;
const midEnergyNames        = 12 ;
const midDivertingRule      = 13 ;
const midForcingRules       = 14 ;
const midForcingManual      = 15 ;
const midResetEnergyTotal   = 16 ;
const midSetVar			    = 17 ;
const midSetAl			    = 18 ;

// Event listener for diverting send buttons
document.querySelectorAll(".sendButtonD").forEach(function(divButton){
    divButton.addEventListener("click" , function(event){
        let index = Number (event.target.id.at (-1)) - 1 ;
        let rule  = diverting[index].rule.value ;
        let off   = diverting[index].off.checked ;
console.log(event.target.id, " Index:" + index, " Off:" + off);
        if (off == true)
        {
            rule = `OFF &  ${rule}` ;
        }
        else
        {
            rule = "ON &  " + rule ;
        }
        // An empy rule is OK for diverting
        let string = "{" +
        '"mid":"'    + midDivertingRule + '",' +
        '"index":"'  + index            + '",' +
        '"rule":"'   + rule             + '"}' ;

        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            console.log ("sendToServer " + array) ;
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Syntax error: ", array [2]) ;
                }
            }
        }) ;
     }) 
})

// Event listener for forcing send buttons
document.querySelectorAll(".sendButton").forEach(function(forceButton){
    forceButton.addEventListener("click" , function(event){
console.log(event.target.id, event.target.id.at (-1));
        let index = Number (event.target.id.at (-1)) - 1 ;
        let start = forcing[index].start.value.trim() ;
        let stop  = forcing[index].stop.value.trim() ;
        let off   = forcing[index].off.checked ;
console.log(event.target.id, " Index:" + index, " Off:" + off + " length: " + start.length + " " + stop.length);

        // An empy rule is forbidden for forcing
        if (start.length == 0 || stop.length == 0)
        {
            common.ackDialog ("Forcing " + (index+1), "Empty rule") ;
        }
        else
        {
            start = (off == true) ? "OFF & " + start : "ON & " + start ;
            let string = "{" +
            '"mid":"'    + midForcingRules  + '",' +
            '"index":"'  + index            + '",' +
            '"start":"'  + start            + '",' +
            '"stop":"'   + stop             + '"}' ;
            sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
//console.log ("sendToServer " + array) ;
                if (array !== null)
                {
                    if (array [0] != "OK")
                    {
                        common.ackDialog ("Syntax error: ", array [2]) ;
                    }
                }
            })
        }
    }) 
})

// Event listener for forcing 'Man' buttons
document.querySelectorAll(".manButton").forEach(function(manButton){
    manButton.addEventListener("click" , function(event){
        let index = Number (event.target.id.at (-1)) - 1 ;

console.log(event.target.id, " Index:" + index);

        let string = "{" +
        '"mid":"'    + midForcingManual + '",' +
        '"index":"'  + index            + '"}' ;    // 0 based
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
//console.log ("sendToServer " + array) ;
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Error: ", array [2]) ;
                }
            }
        })
    }) 
});

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

// Remove the 1st word of the start rule which is "ON" or "OFF" possibly followed by " &"
function trimOnOff (rule)
{
    let word ;
    rule = rule.trim () ;           // Remove begining spaces
    word = getFirstWord (rule) ;
    rule = rule.substring (word.length) ;  // Remove 1st word
    rule = rule.trim () ;       // Remove begining spaces
    word = getFirstWord (rule) ;
    if (word == "&")
    {
        rule = rule.substring (word.length) ;  // Remove & if present
    }
    return rule.trim() ;
}

// Request the diverting/forcing rules from the server
function loadRules ()
{
    let rule ;
    let word ;

    let data = getServerData ("divrules.cgi").then (data => {
        if (data != null)
        {

            rule = data["rule1"].trim () ;
            word = getFirstWord (rule) ;
            diverting[0].off.checked = (word == "OFF") ? true : false ;
            diverting[0].rule.value  = trimOnOff (rule) ;

            rule = data["rule2"].trim () ;
            word = getFirstWord (rule) ;
            diverting[1].off.checked = (word == "OFF") ? true : false ;
            diverting[1].rule.value  = trimOnOff (rule) ;
        }
        else
        {
            diverting[0].off.checked = false ;
            diverting[0].rule.value  = "" ;
            diverting[1].off.checked = false ;
            diverting[1].rule.value  = "" ;
        }
    }) ;
console.log ("divrules.cgi ended") ;
    data = getServerData ("forcerules.cgi").then (data => {
        if (data != null)
        {
            let key  ;

            for (let ii = 0 ; ii < FORCING_MAX ; ii++)
            {
                key = "start" + ii  ;

                if (Object.hasOwn (data, key))
                {
                    rule = data[key].trim () ;
                    word = getFirstWord (rule) ;
                    forcing[ii].off.checked = (word == "OFF") ? true : false ;
                    forcing[ii].start.value = trimOnOff (rule) ;

                    rule = data["stop" + ii] ;
                    forcing[ii].stop.value  = rule ;
                }
                else
                {
                    forcing[ii].off.checked = false ;
                    forcing[ii].start.value = "" ;
                    forcing[ii].stop.value  = "" ;
                }
            }
        }
        else
        {
            for (let ii = 0 ; ii < FORCING_MAX ; ii++)
            {
                forcing[ii].off.checked = false ;
                forcing[ii].start.value = "" ;
                forcing[ii].stop.value  = "" ;
            }
        }
    }) ;
console.log ("forcerules.cgi ended") ;
}

//------------------------------------------------------------------
// Update dfStatus display and diverting / forcing green leds
// let greenLeds = [] ;
// DIVERTING_MAX + FORCING_MAX

function updateDfStatus ()
{
    let data = getServerData ("dfstatus.cgi").then (data => {
        if (data != null)
        {
            let text ;
            let words ;
            let idx ;
            for (let ii = 0 ; ii < OUT_MAX ; ii++)
            {
                text = data["out" + (ii)]
                words = text.split (" ") ;
                idx = Number (words[1]) ;   // Index of the diverting or forcing, 0 based

                // Set led
                if (words [0] == "Diverting")
                {
                    // Diverting are at index 0 and 1 in greenLeds[]
                    dfStatus[ii].display.innerText = words [0] ;

                    if (dfStatus[ii].led != greenLeds [idx])
                    {
                        if (dfStatus[ii].led !== undefined)
                        {
                            dfStatus[ii].led.setAttribute("data-led", "off") ;
                        }
                        greenLeds [idx].setAttribute("data-led", "on");
                        dfStatus[ii].led = greenLeds [idx] ;
                    }
                }
                else if (words [0] == "Forcing")
                {
                    // Forcing indexes are from DIVERTING_MAX in greenLeds[]
                    dfStatus[ii].display.innerText = words [0] + " " + (idx+1) ;

                    idx += DIVERTING_MAX ;
                    if (dfStatus[ii].led != greenLeds [idx])
                    {
                        if (dfStatus[ii].led !== undefined)
                        {
                            dfStatus[ii].led.setAttribute("data-led", "off") ;
                        }
                        greenLeds [idx].setAttribute("data-led", "on");
                        dfStatus[ii].led = greenLeds [idx] ;
                    }
                }
                else
                {
                    // Idle
                    dfStatus[ii].display.innerText = "" ;
                    if (dfStatus[ii].led != undefined)
                    {
                        dfStatus[ii].led.setAttribute("data-led", "off") ;
                        dfStatus[ii].led = undefined ;
                    }
                }
            }
        }
        else
        {
            for (let ii = 0 ; ii < OUT_MAX ; ii++)
            {
                dfStatus[ii].display.innerText = "?" ;
                if (dfStatus[ii].led !== undefined)
                {
                    dfStatus[ii].led.setAttribute("data-led", "off") ;
                    dfStatus[ii].led = undefined ;
                }
            }
        }
    }) ;
console.log ("dfstatus.cgi ended") ;
}


//------------------------------------------------------------------
// For test only: Reload the rules from he server
const reloadButton = document.getElementById ("reloadButton");
reloadButton.addEventListener("click", loadRules);

//------------------------------------------------------------------
// Global diverting enable/disable button

const divOnOffB = document.getElementById ("divOnOffButton");
divOnOffB.addEventListener("click", divOnOffButton)  ;

function divOnOffButton ()
{
    let state = (divOnOffB.value == "on") ? "off" : "on" ;

    setDivOnOff (divOnOffB.value != "on") ; // Invert state
    sendToServer ("setConfig.cgi", JSON.parse ('{"mid":"' + midDivOnOff + '","div":"' + state + '"}'), false).then (array => { }) ;
    // Get the new status of the diverter after a short delay
    setTimeout (() => {
     }, 250) ;
}

function setDivOnOff (state)    // if state == true then diverter on
{
    if (state == false)
    {
        divOnOffB.value = "off" ;
        divOnOffB.innerText = "Off" ;
    }
    else
    {
        divOnOffB.value = "on" ;
        divOnOffB.innerText = "On" ;
    }
}

//------------------------------------------------------------------

function updatePeriodic ()
{
    // Update global diverter button color
    getServerData ("statusWord.cgi").then (data => {
        const wordBit = ["", "X"] ;
        let word = data["SW"] ;
console.log ("statusWord ", word) ;
    
        // Diverter  on/off is bit 0x01
        setDivOnOff (wordBit [(word >>> 0) & 0x01] != 0) ;
console.log ("statusWord.cgi ended") ;

        // Update dfStatus and forcing/diverting running green led
        updateDfStatus () ;
    }) ;
}

//------------------------------------------------------------------
//  Write to Flash button

const writeButton = document.getElementById ("writeButton") ;
writeButton.addEventListener("click", writeButtonClick);

function writeButtonClick()
{
    sendToServer ("setConfig.cgi", JSON.parse ('{"mid":"' + midWrite + '"}'), false).then (array => {
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
// If the user leaves the page without having written the modifications in Flash, warn them
// https://stackoverflow.com/questions/7317273/warn-user-before-leaving-web-page-with-unsaved-changes

window.addEventListener("beforeunload", function (e) {
    var confirmationMessage = "You have attempted to leave this page. Some configuration data is not written to the FLASH. Are you sure you want to exit this page?";
  
    if(common.bModified)
    {
        e.returnValue = confirmationMessage; // Gecko, Trident, Chrome 34+
        return confirmationMessage; // Gecko, WebKit, Chrome <34
    }
    return undefined;
  });

//------------------------------------------------------------------
// '<<' button: Go back to status window

const backButton = document.getElementById ("backButton") ;
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

// Set footer text
document.getElementById ("footerText").innerHTML = common.getFooterString () ;

// Initialize configuration data display
loadRules () ;

// Periodic display update of data
let updateTimer = setInterval (updatePeriodic, 1200) ;

//------------------------------------------------------------------
