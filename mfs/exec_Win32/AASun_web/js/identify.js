
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

let staSsid     = document.getElementById ("staSsid") ;
let staPassword = document.getElementById ("staPassword") ;
let apPassword  = document.getElementById ("apPassword") ;

//------------------------------------------------------------------
// https://dmitripavlutin.com/javascript-fetch-async-await/

// Send data to the server
// Returns null or an array of strings (from the server answer):
//      Success :  "OK mid"
//      Fail:      "Error mid number" 
// If the server isn't reachabe: displays a dialog signaling the error and returns null

async function sendToServer (urlData, stringData)
{
console.log ("sendToServer " + urlData + " " + stringData) ;

    const response = await fetch (urlData, { method: "POST",
                        headers: { "Content-type": "application/json" },
                        body: stringData }) ;
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
// Send Station credentials

const staSendButton = document.getElementById ("staSendButton") ;
staSendButton.addEventListener ("click", () =>
{

    let ssid = staSsid.value ;
    let pswd = staPassword.value ;

    if (ssid.length == 0)
    {
        common.ackDialog ("Error ", "Empty SSID") ;
        return ;
    }
    if (pswd.length == 0)
    {
        common.ackDialog ("Error ", "Empty password") ;
        return ;
    }


    let staString = ssid + " " + pswd ;
console.log ("STA '" + staString + "'") ;

    sendToServer ("wifiCredentialSta.cgi", staString).then (array =>
    {
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Send ", "Error") ;
            }
            else
            {
                common.ackDialog ("Send", "Ok") ;
            }
        }
    })
}) ;

//------------------------------------------------------------------
// Send Access Point password

const apSendButton = document.getElementById("apSendButton") ;
apSendButton.addEventListener("click", () =>
{
    let pswd = apPassword.value ;

    if (pswd.length == 0)
    {
        common.ackDialog ("Error ", "Empty password") ;
        return ;
    }
console.log ("AP PSWD '" + pswd + "'") ;

    sendToServer ("wifiCredentialAp.cgi", pswd).then (array =>
    {
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Send ", "Error") ;
            }
            else
            {
                common.ackDialog ("Send", "Ok") ;
            }
        }
    })
}) ;

//------------------------------------------------------------------
// Reset Access Point password

const apResetButton = document.getElementById("apResetButton") ;
apResetButton.addEventListener("click", () =>
{
    sendToServer ("wifiCredentialApReset.cgi", " ").then (array =>
    {
        if (array !== null)
        {
            if (array [0] != "OK")
            {
                common.ackDialog ("Send ", "Error") ;
            }
            else
            {
                common.ackDialog ("Send", "Ok") ;
            }
        }
    })
}) ;


//------------------------------------------------------------------
//------------------------------------------------------------------

// Need writeLed to use ackDialog(), but don't show it
document.getElementById("writeLed").style.visibility = 'hidden';

// Set footer text
document.getElementById("footerText").innerHTML = common.getFooterString () ;

//------------------------------------------------------------------
 