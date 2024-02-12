
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

const ENERGY_COUNT      = 9 ;           // Count of energies data to display

const HISTO_MAX         = 32 ;          // Max history depth

let histoIndex = 0 ;

//------------------------------------------------------------------
// To conditionnaly show/hide some items on the 1st display

const hideCT3 = document.querySelectorAll (".hide3") ;
const hideCT4 = document.querySelectorAll (".hide4") ;

const showMode = '' ;
const hideMode = 'none' ;
let firstShow = true ;

let hasCT3 = true ;
let hasCT4 = true ;

function setHideShow (list, display)
{
    for (let i = 0 ; i < list.length ; i++)
    {
        list[i].style.display = display ;
    }
}

//------------------------------------------------------------------
//------------------------------------------------------------------

function updatePowerGroup ()
{
    getServerData ("volt.cgi").then (data => {
        if (data === null)
        {
            document.getElementById("vRms").innerHTML   = "-" ;
            document.getElementById("p1Real").innerHTML = "-" ;
            document.getElementById("p1App").innerHTML  = "-" ;
            document.getElementById("p2Real").innerHTML = "-" ;
            document.getElementById("p2App").innerHTML  = "-" ;
            document.getElementById("p3Real").innerHTML = "-" ;
            document.getElementById("p3App").innerHTML  = "-" ;
            document.getElementById("p4Real").innerHTML = "-" ;
            document.getElementById("p4App").innerHTML  = "-" ;
            document.getElementById("pDiv").innerHTML   = "-" ;
            document.getElementById("cPhi1").innerHTML  = "-" ;
            document.getElementById("cPhi2").innerHTML  = "-" ;
            document.getElementById("cPhi3").innerHTML  = "-" ;
            document.getElementById("cPhi4").innerHTML  = "-" ;
            document.getElementById("Counter1").innerHTML  = "-" ;
            document.getElementById("Counter2").innerHTML  = "-" ;
        }
        else
        {
console.log ("updatePowerGroup ", JSON.stringify(data)) ;
            document.getElementById("vRms").innerHTML   = data["vRms"] ;
            document.getElementById("p1Real").innerHTML = data["p1Real"] ;
            document.getElementById("p1App").innerHTML  = data["p1App"] ;
            document.getElementById("p2Real").innerHTML = data["p2Real"] ;
            document.getElementById("p2App").innerHTML  = data["p2App"] ;
            document.getElementById("pDiv").innerHTML   = data["pDiv"] ;

            let cos = Number (data["cPhi1"]) / 1000 ; 
            document.getElementById("cPhi1").innerHTML  = cos.toFixed (3) ;
            cos = Number (data["cPhi2"]) / 1000 ; 
            document.getElementById("cPhi2").innerHTML  = cos.toFixed (3) ;

            document.getElementById("Counter1").innerHTML  = data["Counter1"] ;
            document.getElementById("Counter2").innerHTML  = data["Counter2"] ;

            if (firstShow)
            {
                if (Object.hasOwn (data, "p3Real"))
                {
                    setHideShow (hideCT3, showMode) ;
                    hasCT3 = true ;
                }
                if (Object.hasOwn (data, "p4Real"))
                {
                    setHideShow (hideCT4, showMode) ;
                    hasCT4 = true ;
                }
                firstShow = false ;
            }

            if (hasCT3)
            {
                document.getElementById("p3Real").innerHTML = data["p3Real"] ;
                document.getElementById("p3App").innerHTML  = data["p3App"] ;
                cos = Number (data["cPhi3"]) / 1000 ; 
                document.getElementById("cPhi3").innerHTML  = cos.toFixed (3) ;
        
            }
            if (hasCT4)
            {
                document.getElementById("p4Real").innerHTML = data["p4Real"] ;
                document.getElementById("p4App").innerHTML  = data["p4App"] ;
                cos = Number (data["cPhi4"]) / 1000 ; 
                document.getElementById("cPhi4").innerHTML  = cos.toFixed (3) ;
            }
        }
        updateEnergyTotalGroup () ;
    }) ;
}

function updateEnergyTotalGroup ()
{
    getServerData ("energy.cgi/?mid=Total").then (data => {
        if (data !== null  &&  data["date"] != -1)
        {
console.log ("updateEnergyTotalGroup ", JSON.stringify(data)) ;
            let date = Number (data["date"]) ;
            let string = "" + (date >> 16) + "/" + ((date >> 8) & 0xFF) + "/" + (date & 0xFF) ;
            document.getElementById("etdate").innerHTML = string ;
            document.getElementById("et0").innerHTML  = data["e0"] ;
            document.getElementById("et1").innerHTML  = data["e1"] ;
            document.getElementById("et2").innerHTML  = data["e2"] ;
            document.getElementById("et3").innerHTML  = data["e3"] ;
            document.getElementById("et4").innerHTML  = data["e4"] ;
            if (hasCT3)
            {
                document.getElementById("et5").innerHTML = data["e5"] ;
            }
            if (hasCT4)
            {
                document.getElementById("et6").innerHTML = data["e6"] ;
            }
            document.getElementById("et7").innerHTML = data["e7"] ;
            document.getElementById("et8").innerHTML = data["e8"] ;
        }
        else
        {
            document.getElementById("etdate").innerHTML = "" ;
            document.getElementById("et0").innerHTML = "-" ;
            document.getElementById("et1").innerHTML = "-" ;
            document.getElementById("et2").innerHTML = "-" ;
            document.getElementById("et3").innerHTML = "-" ;
            document.getElementById("et4").innerHTML = "-" ;
            document.getElementById("et5").innerHTML = "-" ;
            document.getElementById("et6").innerHTML = "-" ;
            document.getElementById("et7").innerHTML = "-" ;
            document.getElementById("et8").innerHTML = "-" ;
        }
        updateEnergyTodayGroup () ;
    }) ;
}

function updateEnergyTodayGroup ()
{
    getServerData ("energy.cgi/?mid=Day").then (data => {
        if (data !== null  &&  data["date"] != -1)
        {
console.log ("updateEnergyTodayGroup ", JSON.stringify(data)) ;
            let date = Number (data["date"]) ;
            let string = "" + (date >> 16) + "/" + ((date >> 8) & 0xFF) + "/" + (date & 0xFF) ;
            document.getElementById("eddate").innerHTML = string ;
            document.getElementById("ed0").innerHTML  = data["e0"] ;
            document.getElementById("ed1").innerHTML  = data["e1"] ;
            document.getElementById("ed2").innerHTML  = data["e2"] ;
            document.getElementById("ed3").innerHTML  = data["e3"] ;
            document.getElementById("ed4").innerHTML  = data["e4"] ;
            if (hasCT3)
            {
                document.getElementById("ed5").innerHTML = data["e5"] ;
            }
            if (hasCT4)
            {
                document.getElementById("ed6").innerHTML = data["e6"] ;
            }
            document.getElementById("ed7").innerHTML = data["e7"] ;
            document.getElementById("ed8").innerHTML = data["e8"] ;
        }
        else
        {
            document.getElementById("eddate").innerHTML = "" ;
            document.getElementById("ed0").innerHTML = "-" ;
            document.getElementById("ed1").innerHTML = "-" ;
            document.getElementById("ed2").innerHTML = "-" ;
            document.getElementById("ed3").innerHTML = "-" ;
            document.getElementById("ed4").innerHTML = "-" ;
            document.getElementById("ed5").innerHTML = "-" ;
            document.getElementById("ed6").innerHTML = "-" ;
            document.getElementById("ed7").innerHTML = "-" ;
            document.getElementById("ed8").innerHTML = "-" ;
        }
    })
}

function updateEnergyHistoGroup ()
{
    getServerData ("energy.cgi/?mid=H&index=" + histoIndex).then (data => {
        if (data !== null  &&  data["date"] != -1)
        {
console.log ("updateEnergyHistoGroup ", JSON.stringify(data)) ;
            let date = Number (data["date"]) ;
            let string = "" + (date >> 16) + "/" + ((date >> 8) & 0xFF) + "/" + (date & 0xFF) ;
            document.getElementById("eydate").innerHTML = string ;
            document.getElementById("ey0").innerHTML  = data["e0"] ;
            document.getElementById("ey1").innerHTML  = data["e1"] ;
            document.getElementById("ey2").innerHTML  = data["e2"] ;
            document.getElementById("ey3").innerHTML  = data["e3"] ;
            document.getElementById("ey4").innerHTML  = data["e4"] ;
            if (hasCT3)
            {
                document.getElementById("ey5").innerHTML = data["e5"] ;
            }
            if (hasCT4)
            {
                document.getElementById("ey6").innerHTML = data["e6"] ;
            }
            document.getElementById("ey7").innerHTML = data["e7"] ;
            document.getElementById("ey8").innerHTML = data["e8"] ;
        }
        else
        {
            document.getElementById("eydate").innerHTML = "" ;
            document.getElementById("ey0").innerHTML = "-" ;
            document.getElementById("ey1").innerHTML = "-" ;
            document.getElementById("ey2").innerHTML = "-" ;
            document.getElementById("ey3").innerHTML = "-" ;
            document.getElementById("ey4").innerHTML = "-" ;
            document.getElementById("ey5").innerHTML = "-" ;
            document.getElementById("ey6").innerHTML = "-" ;
            document.getElementById("ey7").innerHTML = "-" ;
            document.getElementById("ey8").innerHTML = "-" ;
        }
    })
}

//------------------------------------------------------------------
//  Get data from the server
//  On fail returns null
//  On success returns a json object with the server data

async function getServerData (urldata)
{
console.log ("getServerData " + urldata) ;
    try{
        const response = await fetch(urldata, { method: "GET", headers: {"Content-Type": "application/json" } }) ;

        if (! response.ok)
        {
            common.ackDialog ("Error", "Status: " + response.status) ;
            return null ;
        }
        const answer = await response.json() ; // response is a promise waiting for a JSON object
        return answer ;     // answer is a JavaScript object
    }
    catch (error)
    {
        return null ;
    }
}

//------------------------------------------------------------------
// https://dmitripavlutin.com/javascript-fetch-async-await/

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

function updateDisplay ()
{
    updatePowerGroup () ;
}

//------------------------------------------------------------------
// Set the names of the energies data

function updateEnergyNames ()
{
    getServerData ("enames.cgi").then (data => {
        if (data != null)
        {
            let ii ;
            for (ii = 0 ; ii < ENERGY_COUNT ; ii++)
            {
                let key = "n" + ii ;
                let name = data[key] ;

                if (Object.hasOwn (data, key))
                {
                    document.getElementById("et" + ii + "name").innerHTML = name ;
                    document.getElementById("ed" + ii + "name").innerHTML = name ;
                    document.getElementById("ey" + ii + "name").innerHTML = name ;
                }
            }
        }
        updateDisplay () ;
    }) ;
}

//------------------------------------------------------------------
//------------------------------------------------------------------

// Default: hide CT3 and CT4
setHideShow (hideCT3, hideMode) ;
setHideShow (hideCT4, hideMode) ;

// Go back to status window
const backButton = document.getElementById("backButton") ;
backButton.addEventListener("click", () => { window.location.assign ("./index.html");}) ;
// backButton.addEventListener("click", () => { history.back(); }) ;

//For test only
// const stsButton = document.getElementById("stsButton");
// stsButton.addEventListener("click", toggle34);

// Configure the history input
let histoDay   = document.getElementById("histoDay");
histoDay.min   = 0 ;
histoDay.max   = HISTO_MAX - 2 ;
histoDay.value = histoIndex ;

const getHistoButton = document.getElementById("getHistoButton") ;
getHistoButton.addEventListener("click", () => { 
    histoIndex = histoDay.value ;
    updateEnergyHistoGroup () ;
}) ;

const moreHistoButton = document.getElementById("moreHistoButton") ;
moreHistoButton.addEventListener("click", () => { 
    histoIndex = Number (histoDay.value) ;
    histoIndex += 1 ;
    if (histoIndex < HISTO_MAX-1)
    {
        histoDay.value = histoIndex ;
    }
}) ;

const lessHistoButton = document.getElementById("lessHistoButton") ;
lessHistoButton.addEventListener("click", () => { 
    histoIndex = Number (histoDay.value) ;
    if (histoIndex > 0)
    {
        histoDay.value = histoIndex - 1 ;
    }
}) ;

const resetTotalButton = document.getElementById("resetTotalButton") ;
resetTotalButton.addEventListener("click", () => { 

    let string = '{"mid":"' + midResetEnergyTotal + '"}' ;
    sendToServer ("setConfig.cgi", JSON.parse (string), false).then (array => {
        console.log ("sendToServer " + array) ;
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Syntax error: ", array [2]) ;
            }
        }
    }) ;
}) ;

//------------------------------------------------------------------

// Set footer text
document.getElementById("footerText").innerHTML = common.getFooterString () ;

// Request the names of the energies
// Then request the configuration data
updateEnergyNames () ;

setTimeout(() => {
    updateEnergyHistoGroup() ;
  }, "500");

//  For periodic update of the page
let updateTimer = setInterval (updateDisplay, 1200) ;

//------------------------------------------------------------------
 