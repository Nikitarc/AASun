
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

let varSelect = document.getElementById("varSelect") ;
let varValue  = document.getElementById("varValue") ;

let alSelect  = document.getElementById("alSelect") ;
let alValue   = document.getElementById("alValue") ;

let v1 = document.getElementById("v1") ;
let v2 = document.getElementById("v2") ;
let v3 = document.getElementById("v3") ;
let v4 = document.getElementById("v4") ;

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

function updateData (bVar, bAl)
{
    getServerData ("variable.cgi").then (data => {
        if (data != null)
        {
console.log ("displayConfig ", JSON.stringify(data)) ;
            if (bVar == true)
            {
                v1.innerHTML = data ["V1"] ;
                v2.innerHTML = data ["V2"] ;
                v3.innerHTML = data ["V3"] ;
                v4.innerHTML = data ["V4"] ;
            }

            if (bAl == true)
            {
                let string = data ["alSensor"] ;
                alSelect.value = string ;
                if (string.charAt(0) == "O")
                {
                    // Anti-legionella Off
                    alValue.value = "" ;
                }
                else
                {
                    alValue.value = data ["alValue"] ;
                }
            }
        }
        else
        {
            if (bVar == true)
            {
                v1.innerHTML = "" ;
                v2.innerHTML = "" ;
                v3.innerHTML = "" ;
                v4.innerHTML = "" ;
            }
            if (bAl == true)
            {
                alSelect.value = "0" ;
                alValue.value = "" ;
            }
        }
    }) ;
}

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
// Send variable button

const varSendButton = document.getElementById("varSendButton") ;
varSendButton.addEventListener("click", () => {
    console.log (varSelect.value + " " + varValue.value) ;
    let string = '{"mid":"' + common.midSetVar  + '",' +
                 '"' + varSelect.value + '":"' + varValue.value + '"}' ;
console.log (string) ;
    sendToServer ("setConfig.cgi", JSON.parse (string), false).then (array => {
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Server error: ", array [2]) ;
            }
        }
    })
}) ;

//------------------------------------------------------------------
// Send Anti Legionella

const alSendButton = document.getElementById("alSendButton") ;
alSendButton.addEventListener("click", () => {
    console.log (alSelect.value + " " + alValue.value) ;
    if (alSelect.value != "0")
    {
        let string = '{"mid":"' + common.midSetAl  +
                    '","sensor":"' + alSelect.value + 
                    '","value":"'  + alValue.value + '"}' ;
console.log (string) ;
        sendToServer ("setConfig.cgi", JSON.parse (string), true).then (array => {
            if (array !== null)
            {
                if (array [0] != "OK")
                {
                    common.ackDialog ("Server error: ", array [2]) ;
                }
                else
                {
                    setTimeout(() => {
                        updateData (false, true) ;
                      }, "200");
                }
            }
        })
    }
}) ;

//------------------------------------------------------------------
// Go back to status window

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
// backButton.addEventListener("click", () => { history.back(); }) ;

//------------------------------------------------------------------
// For test only

// const stsButton = document.getElementById("stsButton");
// stsButton.addEventListener("click", () => {
//     console.log ("stsButton") ;
//     common.switchLed (); 
// }) ; 

//------------------------------------------------------------------
//------------------------------------------------------------------

// Set footer text
document.getElementById("footerText").innerHTML = common.getFooterString () ;

// Triggers 1st data update
setTimeout(() => {
     updateData (false, true) ;
   }, "200");

//  For periodic update of the page
// let updateTimer = setInterval (updateData (true, false), 1200) ;
let updateTimer = setInterval (function() { updateData (true, false); }, 1200) ;

//------------------------------------------------------------------
 