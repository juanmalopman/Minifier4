// Runs when page loads.
(function()
{
	// Requests JSON containing first 6 investment's miniatures
	fetch('firstSixProperties.json')
		.then(function(response) {
			// The response is a Response instance.
			// You parse the data into a useable format using `.json()`
			return response.json();
		}).then(function(data) {
			// `data` is the parsed version of the JSON returned from the above endpoint.
			// console.log(data);  // { "id": 1, "status": "VENDIDO", "picture": "/9j/4QAYRXhpZgAASUkqAAgAAAAAAAA..." }
			var array = Object.values(data);
			for (var i = 0; i < 6; i++)
			{
				var propertyToChange = document.getElementById("investments").children[i];
				propertyToChange.children[0].style.backgroundImage = 'url("data:image/jpg;base64,'+array[i].picture+'")';
				propertyToChange.children[1].innerHTML = array[i].state;
				propertyToChange.children[2].innerHTML = array[i].description;
			};
		});


		//.then((response) => 

})();

var navBar = document.getElementById("navBar");
var navBarFirstChild = navBar.firstChild;

navBar.firstChild.onpointerdown = closeMenu;

function closeMenu()
{
	// If the viewport is wide, disable the menu functionality.
	if (getStyle(navBarFirstChild,"padding-top") != "90px") { return; }
	if (navBarFirstChild.classList.contains(/*.classNext*/"closedNavBarMenu"))
	{
		navBarFirstChild.classList.remove(/*.classNext*/"closedNavBarMenu");
		navBarFirstChild.style.maxHeight = "270px";
	}
	else
	{
		navBarFirstChild.classList.add(/*.classNext*/"closedNavBarMenu");
		navBarFirstChild.style.maxHeight = "90px";
	}
};

navBar.firstChild.firstChild.onpointerdown = function (e)
{
	// Touching the NavBar branding doesn't trigger menu actions.
	e.stopPropagation();
};


document.getElementById("navbarToInvestments").onpointerdown=function(e){e.stopPropagation();slideToId(/*.idNext*/"investmentsSectionStart");};
document.getElementById("footerToInvestments").onpointerdown=function(){slideToId(/*.idNext*/"investmentsSectionStart");};
document.getElementById("navbarToLegal").onpointerdown=function(e){e.stopPropagation();slideToId(/*.idNext*/"legalExplained");};
document.getElementById("footerToLegal").onpointerdown=function(){slideToId(/*.idNext*/"legalExplained");};

function slideToId(idToSlideTo)
{
	var offsetBody = document.documentElement.scrollTop;
	// Suma 60px porque todos los divs a los que scrolleás tienen un spacer superior de 60px.
	var deltaY = (offsetBody - document.getElementById(idToSlideTo).offsetTop)/-100;
	// Se fija si está abierto el menú navExpandable.
	if ( navBar.clientHeight > 111 && navBar.clientWidth < 631 )
	{
		// No le quita a la posición actual del scroll lo que va a necesitar compensar al cerrar el menu porq el menú no empuja nada.
		// offsetBody-=180;
		// Cierra el menu.
		closeMenu();
	};
	slideToIdHelper(offsetBody,deltaY,99);
};


function slideToIdHelper(offsetBody,deltaY,slideToClassHelperIterations)
{
	setTimeout(function() {
		offsetBody+=deltaY;
		document.documentElement.scrollTo(0, offsetBody);
		if(slideToClassHelperIterations--)
		{
			slideToIdHelper(offsetBody,deltaY,slideToClassHelperIterations);
		}
	},2); //200ms total animation
};

// Function that returns elements desired property value regardless of it's presence in the DOM or a stylesheet
function getStyle(element, name)
{
	return window.getComputedStyle(element, null).getPropertyValue(name);
};