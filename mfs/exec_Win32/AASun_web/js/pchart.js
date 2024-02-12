
// This imports a namespace: to use imported objects they must be prefixed with 'common.'
// E.G.: common.versionString
import * as common  from '/js/common.js';

console.log ("chart start") ;

const POWER_ITEM_MAX    = 9 ;           // Count ot 32 bit values in a sample
const TODAY_HISTO       = 1 ;
const YESTERDAY_HISTO   = 3 ;
const POWER_HISTO_MAGIC	= 0x12345678 ;	// To check data header validity

const ENERGY_COUNT      = 9 ;           // Count of curves to display

const HISTO_MAX         = 32 ;          // Max history depth

let histoIndex = 0 ;

// Create a line chart
var chart = anychart.line() ;
var chartData = [] ;
var dataHh = 0 ;
var dataMm = 0 ;
var histoDate = "" ;
//var typeHisto = TODAY_HISTO ;

// In the data from the server the 3 1st power are always available (imported, exported, diverted).
// Others are optionnal.
var powerAvailable = [ 1, 1, 1, 1, 0, 0, 0, 0, 0] ;

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
// Set the names of the chart series

function updateEnergyNames (data)
{
    for (let ii = 0 ; ii < ENERGY_COUNT ; ii++)
    {
        let key = "n" + ii ;
        if (Object.hasOwn (data, key))
        {
            chart.getSeriesAt(ii).name(data[key]) ;
        }
    }

    // Get chart data for the 1st time
    serverHistoRequest (TODAY_HISTO) ;
}

//------------------------------------------------------------------
// The server have a buffer of only 2 kB, not enough for the full history.
// Then the server allows to acquire it in 2 parts.

// Example:             url = 'powerHisto.cgi/?mid=3&index=2'; 

async function serverHistoRequest (typeHisto)
{
    histoIndex = histoDay.value ;
    try
    {
console.log ("serverHistoRequest " + typeHisto) ;
        // Get the 1st part of the data
        let url ;
        url = 'powerHisto.cgi' ;
        url += '?mid=' + typeHisto ;
        url += '&index=' + histoIndex ;

        let response = await fetch (url, { method: "GET", headers: {"Content-Type": "application/json" } }) ;
        if (response.ok == false)
        {
            throw "Not ok, status " + response.status ;
        }
        let arrayBuffer = await response.arrayBuffer() ;    // arrayBuffer is pure binary data (bytes)
        if (0 == buildData (arrayBuffer, 1))
        {
            ackDialog ("History", "Data not available") ;
            throw "Data not available" ;
        }

        // Get the 2nd part of the data
        url = 'powerHisto.cgi' ;
        url += '?mid=' + (typeHisto + 1) ;
        url += '&index=' + histoIndex ;
        response = await fetch (url, { method: "GET", headers: {"Content-Type": "application/json" } }) ;
        if (response.ok == false)
        {
            throw "Not ok, status " + response.status ;
        }
        arrayBuffer = await response.arrayBuffer() ;    // arrayBuffer is pure binary data (bytes)
        buildData (arrayBuffer, 2) ;
        updateChartData () ;
        chart.draw() ;
    }
    catch(error)
    {
console.log ("serverHistoRequest catch Error : " + error) ;
        chartData.length = 0 ;  // Empty array
        updateChartData () ;
        chart.draw() ;
    };
console.log ("serverHistoRequest ended") ;
}

//------------------------------------------------------------------

function buildData (arrayBuffer, step)
{
    let chartItem = [] ;
    let offset ;
    let ii, jj ;
    let data = new Int32Array (arrayBuffer) ;

console.log ("arrayBuffer length " + arrayBuffer.byteLength) ;
console.log ("data length " + data.length) ;
    if (step === 1)
    {
        // 1st part of the history data
        // Check header validity
        if (data [0] != POWER_HISTO_MAGIC)
        {
            return 0 ;  // Error, this is not the requested data
        }
        chartData.length = 0 ;  // Empty array
        dataHh = 0 ;
        dataMm = 0 ;
        ii     = 1 ;    // Skip header
        offset = POWER_ITEM_MAX ;

        // Build date
        histoDate = (data [1] >> 16) + "/" + ((data [1] >> 8) & 0xFF) + "/" + (data [1] & 0xFF) ;

        // Build powerAvailable
        for (jj = 4 ; jj < POWER_ITEM_MAX ; jj++)
        {
            powerAvailable [jj] = data [jj] ;
        }
console.log (powerAvailable) ;
    }
    else
    {
        // 2nd part of the history data
        ii = 0 ;
        offset = 0 ;
    }
    for (  ; ii < data.length / POWER_ITEM_MAX ; ii++)
    {
        chartItem.length = 0 ;
        chartItem.push (dataHh.toString () + ':' + dataMm.toString().padStart(2, '0')) ;    // The x axis label
        for (jj = 0 ; jj < POWER_ITEM_MAX ; jj++)
        {
            chartItem.push (data [offset]) ;
            offset++ ;
        }
        dataMm += 15 ;
        if (dataMm == 60)
        {
            dataHh++ ;
            dataMm = 0 ;
        }
        chartData.push (chartItem.slice()) ; // Slice to create a new copy of chartItem
    }
    return 1 ;      // OK
}

//------------------------------------------------------------------
//  Update the chart internal data representation with the content of chartData

function updateChartData ()
{
console.log ("updateChart " + chartData.length) ;
    // create a data set
    var dataSet = anychart.data.set(chartData);

    // map the data for all series: defines the index in dataset for the x label and the serie value
    var p1Data = dataSet.mapAs ({x: 0, value: 1}) ;   // x label in item 0, value in item 1
    chart.getSeriesAt (0).data (p1Data) ;             // Update serie data

    var p2Data = dataSet.mapAs ({x: 0, value: 2}) ;   // x label in item 0, value in item 2
    chart.getSeriesAt (1).data (p2Data) ;

    var p3Data = dataSet.mapAs ({x: 0, value: 3}) ;
    chart.getSeriesAt (2).data (p3Data) ;

    var p4Data = dataSet.mapAs ({x: 0, value: 4}) ;
    chart.getSeriesAt (3).data (p4Data) ;

    // P2
    if (powerAvailable [4] != 0)
    {
        var p5Data = dataSet.mapAs ({x: 0, value: 5}) ;
        chart.getSeriesAt (4).data (p5Data) ;
        chart.getSeriesAt (4).enabled (true) ;
    }
    else
    {
        chart.getSeriesAt (4).data (null) ;
        chart.getSeriesAt (4).enabled (false) ;
    }

    // P3
    if (powerAvailable [5] != 0)
    {
        var p6Data = dataSet.mapAs ({x: 0, value: 6}) ;
        chart.getSeriesAt (5).data (p6Data) ;
        chart.getSeriesAt (5).enabled (true) ;
    }
    else
    {
        chart.getSeriesAt (5).data (null) ;
        chart.getSeriesAt (5).enabled (false) ;
    }

    // P4
    if (powerAvailable [6] != 0)
    {
        var p7Data = dataSet.mapAs ({x: 0, value: 7}) ;
        chart.getSeriesAt (6).data (p7Data) ;
        chart.getSeriesAt (6).enabled (true) ;
    }
    else
    {
        chart.getSeriesAt (6).data (null) ;
        chart.getSeriesAt (6).enabled (false) ;
    }

    // Counter 1
    if (powerAvailable [7] != 0)
    {
        var p8Data = dataSet.mapAs ({x: 0, value: 8}) ;
        chart.getSeriesAt (7).data (p8Data) ;
        chart.getSeriesAt (7).enabled (true) ;
    }
    else
    {
        chart.getSeriesAt (7).data (null) ;
        chart.getSeriesAt (7).enabled (false) ;
    }

    // Counter 2
    if (powerAvailable [8] != 0)
    {
        var p9Data = dataSet.mapAs ({x: 0, value: 9}) ;
        chart.getSeriesAt (8).data (p9Data) ;
        chart.getSeriesAt (8).enabled (true) ;
    }
    else
    {
        chart.getSeriesAt (8).data (null) ;
        chart.getSeriesAt (8).enabled (false) ;
    }

    // Update title with the date (only if there is data)
    if (chartData.length != 0)
    {
        chart.title ("AASun Power - " + histoDate) ;
    }
}

//------------------------------------------------------------------
// Create and configure the chart

function createChart ()
{
    // set the interactivity mode
    chart.interactivity().hoverMode("single");

    // Create the series (without data) and add default name (can use spline instead of line)
    var p1Series = chart.spline();        p1Series.name("Imported");
    var p2Series = chart.spline();        p2Series.name("Exported");
    var p3Series = chart.spline();        p3Series.name("Div 1 (est)");
    var p4Series = chart.spline();        p4Series.name("Div 2 (est)");
    var p5Series = chart.spline();        p5Series.name("P2");
    var p6Series = chart.spline();        p6Series.name("P3");
    var p7Series = chart.spline();        p7Series.name("P4");
    var p8Series = chart.spline();        p8Series.name("Cnt1");
    var p9Series = chart.spline();        p9Series.name("Cnt2");

    // Default: Optionnal series are disabled. Will be enabled on available data.
    p5Series.enabled (false) ;
    p6Series.enabled (false) ;
    p7Series.enabled (false) ;
    p8Series.enabled (false) ;
    p9Series.enabled (false) ;

    // Serie line color
    // stroke(colorSettings, thickness, dashSetting, lineJoin, lineCap)
    p1Series.normal().stroke   ("#000000") ;     // 64B5F6
    p1Series.hovered().stroke  ("#000000", 2) ;
    p1Series.selected().stroke ("#000000", 3) ;

    p2Series.normal().stroke   ("#ff0000") ;
    p2Series.hovered().stroke  ("#ff0000", 2) ;
    p2Series.selected().stroke ("#ff0000", 3) ;

    p3Series.normal().stroke   ("#008000") ;
    p3Series.hovered().stroke  ("#008000", 2) ;
    p3Series.selected().stroke ("#008000", 3) ;

    p4Series.normal().stroke   ("#FFDF00") ;
    p4Series.hovered().stroke  ("#FFDF00", 2) ;
    p4Series.selected().stroke ("#FFDF00", 3) ;

    p5Series.normal().stroke   ("#00DDDD") ;
    p5Series.hovered().stroke  ("#00DDDD", 2) ;
    p5Series.selected().stroke ("#00DDDD", 3) ;

    p6Series.normal().stroke   ("#8000FF") ;
    p6Series.hovered().stroke  ("#8000FF", 2) ;
    p6Series.selected().stroke ("#8000FF", 3) ;

    p7Series.normal().stroke   ("#FF5EAE") ;
    p7Series.hovered().stroke  ("#FF5EAE", 2) ;
    p7Series.selected().stroke ("#FF5EAE", 3) ;

    p8Series.normal().stroke   ("#00DD00") ;
    p8Series.hovered().stroke  ("#00DD00", 2) ;
    p8Series.selected().stroke ("#00DD00", 3) ;
 
    p9Series.normal().stroke   ("#0000FF") ;
    p9Series.hovered().stroke  ("#0000FF", 2) ;
    p9Series.selected().stroke ("#0000FF", 3) ;
 
    // Add a legend
    chart.legend().enabled(true);
    chart.legend().itemsLayout("horizontal-expandable");

    // Add a title
    chart.title("AASun Power");

    // Name the axes
    chart.yAxis().title("Watt");
 //   chart.xAxis().title("Hour");

    // Customize the series markers
//    p1Series.hovered().markers().type("circle").size(4);
    p1Series.hovered().markers().size(3);
    p2Series.hovered().markers().size(3);
    p3Series.hovered().markers().size(3);
    p4Series.hovered().markers().size(3);
    p5Series.hovered().markers().size(3);
    p6Series.hovered().markers().size(3);
    p7Series.hovered().markers().size(3);
    p8Series.hovered().markers().size(3);
    p9Series.hovered().markers().size(3);

    // Turn on crosshairs and remove the y hair
    chart.crosshair().enabled(true).yStroke(null).yLabel(false);

    // Change the tooltip position
    chart.tooltip().positionMode("point");
    chart.tooltip().position("right").anchor("left-center").offsetX(5).offsetY(5);
    chart.tooltip().fontSize(10);
    chart.tooltip().title().fontSize(10);

    chart.xScale().mode('continuous');  // remove the gaps to the right and left of the line

    chart.xScroller(true);
    chart.xScroller().position("afterAxes");
    // chart.xScroller().position("beforeAxes");
    chart.xScroller().height(7);
    // chart.xScroller().maxHeight(5);
    chart.xScroller().thumbs().autoHide(true);

    chart.background().fill("rgb(255, 255, 255)");
    chart.background().stroke("rgb(  0,  83,  28)");  
 
    // specify where to display the chart
    chart.container("chartContainer");
}
/*
// Save the chart as png
function pngButton ()
{
    chart.saveAsPng({"width": document.getElementById("chartContainer").width,
                     "height": document.getElementById("chartContainer").height,
                     "quality": 1,
                     "filename": "AASun_Power_" + histoDate});   // .png is added automatically
};
*/
//------------------------------------------------------------------
//------------------------------------------------------------------

// Configure the history input
let histoDay   = document.getElementById("histoDay");
histoDay.min   = 0 ;
histoDay.max   = HISTO_MAX - 2 ;
histoDay.value = histoIndex ;

const moreHistoButton = document.getElementById("moreHistoButton") ;
moreHistoButton.addEventListener("click", () => { 
    histoIndex = Number (histoDay.value) ;
    if (histoIndex < HISTO_MAX-2)
    {
        histoIndex ++ ;
        histoDay.value = histoIndex ;
    }
}) ;

const lessHistoButton = document.getElementById("lessHistoButton") ;
lessHistoButton.addEventListener("click", () => { 
    histoIndex = Number (histoDay.value) ;
    if (histoIndex > 0)
    {
        histoIndex-- ;
        histoDay.value = histoIndex ;
    }
}) ;

const getTodayButton = document.getElementById("getTodayButton") ;
getTodayButton.addEventListener("click", () => { 
    histoIndex = histoDay.value ;
    serverHistoRequest (TODAY_HISTO) ;
}) ;

const getHistoButton = document.getElementById("getHistoButton") ;
getHistoButton.addEventListener("click", () => { 
    histoIndex = histoDay.value ;
    serverHistoRequest (YESTERDAY_HISTO) ;
}) ;

// Save the chart as png
const pngButton = document.getElementById("pngButton") ;
pngButton.addEventListener("click", () => { 
    chart.saveAsPng({"width": document.getElementById("chartContainer").width,
                     "height": document.getElementById("chartContainer").height,
                     "quality": 1,
                     "filename": "AASun_Power_" + histoDate});   // .png is added automatically
}) ;

// Go back to status window
const backButton = document.querySelector(".backButton") ;
backButton.addEventListener("click", () => { window.location.assign ("./index.html");}) ;
// backButton.addEventListener("click", () => { history.back(); }) ;

const vertLegendButton = document.getElementById("vertLegend") ;
vertLegendButton.addEventListener("change", () => { 
    chart.legend().itemsLayout("vertical");
    chart.legend().position("right");
    chart.legend().align("top");
}) ;

const horLegendButton = document.getElementById("horLegend") ;
horLegendButton.addEventListener("change", () => { 
    chart.legend().itemsLayout("horizontal-expandable");
    chart.legend().position("top");
    chart.legend().align("center");
}) ;

//------------------------------------------------------------------

// Set footer text
document.getElementById("footerText").innerHTML = common.getFooterString () ;

anychart.onDocumentReady( () =>
    {
        createChart () ;

        // Request the names of the chart series
        // Then request the chart data
        getServerData ("enames.cgi").then (data =>
        {
            if (data != null)
            {
                updateEnergyNames (data) ;
            }
            else
            {
                console.log ("enames.cgi failed")   // Server not reachable
            }
        }) ;
    }) ;

//------------------------------------------------------------------
